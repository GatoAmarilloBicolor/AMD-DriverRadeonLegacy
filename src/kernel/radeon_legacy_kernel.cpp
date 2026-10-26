/*
 * radeon_legacy_kernel.cpp - Standalone Haiku kernel module.
 *
 * Hardware paths implemented for real here:
 *   - PCI probe of the supported device list (src/common/rg_pci_ids.h)
 *   - MMIO BAR mapping (BAR2 pre-Bonaire, BAR5 Bonaire+) and VRAM aperture
 *   - CONFIG_MEMSIZE VRAM sizing per family
 *   - interrupt attach/detach, PCI config space, area translation
 *
 * Deliberately not implemented here:
 *   - AtomBIOS modesetting, DDC/EDID, DPMS and page flip. The ioctl handlers
 *     are wired but return B_NOT_SUPPORTED (or a single bootstrap CRTC) and
 *     are the integration point described in docs/HAIKU_INTEGRATION.md.
 */
#include "radeon_legacy_kernel.h"

#include <KernelExport.h>
#include <OS.h>
#include <PCI.h>
#include <module.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/rg_pci_ids.h"
#include "../common/rg_types.h"
#include "../helper/helper_ioctls.h"
#include "../server/asic/rg_asic.h"

/* R300-R500 VRAM size (confirmed in Haiku's native radeon driver). */
#define RG_R300_CONFIG_MEMSIZE		0x00f8
#define RG_R300_CONFIG_MEMSIZE_MASK	0x1ff00000
/* R600..pre-Tahiti VRAM size (confirmed in Haiku's radeon_hd driver). */
#define RG_R600_CONFIG_MEMSIZE		0x5428

#define RG_FALLBACK_VRAM_SIZE		(256ull << 20)
#define RG_MAX_PLAUSIBLE_VRAM		(16ull << 30)

struct RadeonLegacyGpu {
	bool			present;
	bool			mapped;
	pci_info		pci;
	const rg_asic_desc	*asic;
	char			name[64];

	area_id			mmioArea;
	area_id			fbArea;
	volatile uint8	*mmio;
	uint8			*fb;
	uint64			mmioPhys;
	uint64			mmioMapSize;
	uint64			fbPhys;
	uint64			fbMapSize;
	uint64			vramSize;

	uint8			irq;
	bool			irqAttached;
	sem_id			irqSem;
	uint32			irqStatusReg;
	uint32			irqMask;

	int32			openCount;
};

static pci_module_info	*sPCI = NULL;
static RadeonLegacyGpu	sGpus[RG_LEGACY_MAX_DEVICES];
static int32		sGpuCount = 0;
static const char	*sDeviceNames[RG_LEGACY_MAX_DEVICES + 1];

static status_t		open_hook(const char *name, uint32 flags, void **cookie);
static status_t		close_hook(void *cookie);
static status_t		free_hook(void *cookie);
static status_t		control_hook(void *cookie, uint32 op, void *buffer,
						size_t length);
static status_t		read_hook(void *cookie, off_t pos, void *buffer,
						size_t *length);
static status_t		write_hook(void *cookie, off_t pos, const void *buffer,
						size_t *length);
static int32		irq_handler(void *data);

static device_hooks sDeviceHooks = {
	open_hook,
	close_hook,
	free_hook,
	control_hook,
	read_hook,
	write_hook,
	NULL,
	NULL,
	NULL,
	NULL
};

extern "C" {
int32 api_version = B_CUR_DRIVER_API_VERSION;
}


static inline uint32
mmio_read(RadeonLegacyGpu *gpu, uint32 offset)
{
	return *(volatile uint32 *)(gpu->mmio + offset);
}


static inline void
mmio_write(RadeonLegacyGpu *gpu, uint32 offset, uint32 value)
{
	*(volatile uint32 *)(gpu->mmio + offset) = value;
}


/* Bonaire and newer expose MMIO in BAR5; older parts use BAR2. */
static uint32
mmio_bar(const pci_info &info, const rg_asic_desc *asic)
{
	if (info.device_id >= 0x9830)
		return 5;
	if (asic != NULL && asic->family >= RG_FAMILY_GCN1)
		return 5;
	return 2;
}


static uint64
read_vram_size(RadeonLegacyGpu *gpu)
{
	uint32 value = 0;
	uint64 size = 0;

	switch (gpu->asic != NULL ? gpu->asic->family : RG_FAMILY_UNKNOWN) {
		case RG_FAMILY_R300:
			value = mmio_read(gpu, RG_R300_CONFIG_MEMSIZE);
			size = value & RG_R300_CONFIG_MEMSIZE_MASK;
			break;

		case RG_FAMILY_R600:
			size = mmio_read(gpu, RG_R600_CONFIG_MEMSIZE);
			break;

		case RG_FAMILY_EVERGREEN:
		case RG_FAMILY_NI:
			value = mmio_read(gpu, RG_R600_CONFIG_MEMSIZE);
			/* APUs report bytes; discrete Evergreen reports megabytes. */
			if (gpu->asic != NULL && gpu->asic->is_apu)
				size = value;
			else
				size = (uint64)value * 1024 * 1024;
			break;

		default:
			break;
	}

	if (size == 0 || size > RG_MAX_PLAUSIBLE_VRAM)
		size = gpu->fbMapSize;
	if (size == 0 || size > RG_MAX_PLAUSIBLE_VRAM)
		size = RG_FALLBACK_VRAM_SIZE;

	return size;
}


static status_t
map_gpu(RadeonLegacyGpu *gpu)
{
	if (gpu->mapped)
		return B_OK;

	uint32 bar = mmio_bar(gpu->pci, gpu->asic);
	uint32 mmioPhys = gpu->pci.u.h0.base_registers[bar];
	uint32 mmioSize = gpu->pci.u.h0.base_register_sizes[bar];

	if (mmioPhys == 0 || mmioSize == 0) {
		/* Fall back to the other BAR before giving up. */
		bar = (bar == 2) ? 5 : 2;
		mmioPhys = gpu->pci.u.h0.base_registers[bar];
		mmioSize = gpu->pci.u.h0.base_register_sizes[bar];
	}
	if (mmioPhys == 0 || mmioSize == 0)
		return B_DEVICE_NOT_FOUND;

	void *mmio = NULL;
	status_t status = map_physical_memory("radeon legacy mmio",
		(phys_addr_t)mmioPhys, mmioSize, B_UNCACHED_MEMORY,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA | B_CLONEABLE_AREA, &mmio);
	if (status != B_OK)
		return status;

	uint32 fbPhys = gpu->pci.u.h0.base_registers[0];
	uint32 fbSize = gpu->pci.u.h0.base_register_sizes[0];
	if (fbPhys == 0 || fbSize == 0) {
		delete_area(area_for(mmio));
		return B_DEVICE_NOT_FOUND;
	}

	void *fb = NULL;
	status = map_physical_memory("radeon legacy vram", (phys_addr_t)fbPhys,
		fbSize, B_WRITE_COMBINING_MEMORY,
		B_READ_AREA | B_WRITE_AREA | B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA
			| B_CLONEABLE_AREA, &fb);
	if (status != B_OK) {
		delete_area(area_for(mmio));
		return status;
	}

	gpu->mmioArea = area_for(mmio);
	gpu->fbArea = area_for(fb);
	gpu->mmio = (volatile uint8 *)mmio;
	gpu->fb = (uint8 *)fb;
	gpu->mmioPhys = mmioPhys;
	gpu->mmioMapSize = mmioSize;
	gpu->fbPhys = fbPhys;
	gpu->fbMapSize = fbSize;
	gpu->vramSize = read_vram_size(gpu);
	gpu->irq = gpu->pci.u.h0.interrupt_line;
	gpu->mapped = true;

	dprintf("radeon_legacy: %s mmio=%" B_PRIu32 "@%#" B_PRIx32
		" vram=%" B_PRIu64 " irq=%u\n", gpu->name, mmioSize, mmioPhys,
		gpu->vramSize, gpu->irq);
	return B_OK;
}


static void
unmap_gpu(RadeonLegacyGpu *gpu)
{
	if (!gpu->mapped)
		return;

	if (gpu->irqAttached) {
		remove_io_interrupt_handler(gpu->irq, irq_handler, gpu);
		gpu->irqAttached = false;
	}

	delete_area(gpu->fbArea);
	delete_area(gpu->mmioArea);
	gpu->fbArea = -1;
	gpu->mmioArea = -1;
	gpu->fb = NULL;
	gpu->mmio = NULL;
	gpu->mapped = false;
}


static int32
irq_handler(void *data)
{
	RadeonLegacyGpu *gpu = (RadeonLegacyGpu *)data;

	if (gpu == NULL || !gpu->irqAttached || gpu->irqStatusReg == 0)
		return B_UNHANDLED_INTERRUPT;

	uint32 status = mmio_read(gpu, gpu->irqStatusReg);
	if ((status & gpu->irqMask) == 0)
		return B_UNHANDLED_INTERRUPT;

	/* Write-one-to-clear is the common Radeon behaviour [VERIFICAR]. */
	mmio_write(gpu, gpu->irqStatusReg, status & gpu->irqMask);
	release_sem_etc(gpu->irqSem, 1, B_DO_NOT_RESCHEDULE);
	return B_HANDLED_INTERRUPT;
}


static int32
find_gpu_index(const char *name)
{
	for (int32 i = 0; i < sGpuCount; i++) {
		if (strcmp(name, sGpus[i].name) == 0)
			return i;
	}
	return -1;
}


static status_t
probe_devices(void)
{
	sGpuCount = 0;
	for (int32 i = 0; i <= RG_LEGACY_MAX_DEVICES; i++)
		sDeviceNames[i] = NULL;

	if (sPCI == NULL)
		return B_NO_INIT;

	for (int32 index = 0; sGpuCount < RG_LEGACY_MAX_DEVICES; index++) {
		pci_info info;
		if (sPCI->get_nth_pci_info(index, &info) != B_OK)
			break;

		if (info.vendor_id != RG_PCI_VENDOR_AMD
			|| info.class_base != PCI_display)
			continue;
		if (!rg_pci_id_is_supported(info.device_id))
			continue;

		RadeonLegacyGpu *gpu = &sGpus[sGpuCount];
		memset(gpu, 0, sizeof(*gpu));
		gpu->present = true;
		gpu->pci = info;
		gpu->mmioArea = -1;
		gpu->fbArea = -1;
		gpu->irqSem = -1;

		uint32 subsystem = ((uint32)info.u.h0.subsystem_vendor_id << 16)
			| info.u.h0.subsystem_id;
		gpu->asic = rg_asic_lookup(info.device_id, info.revision, subsystem);

		snprintf(gpu->name, sizeof(gpu->name), "%s%02x%02x%02x",
			RG_LEGACY_DEVICE_PREFIX, info.bus, info.device, info.function);
		sDeviceNames[sGpuCount] = gpu->name;

		dprintf("radeon_legacy: found %s (%04x:%04x)\n", gpu->name,
			info.vendor_id, info.device_id);
		sGpuCount++;
	}

	return sGpuCount > 0 ? B_OK : B_DEVICE_NOT_FOUND;
}


extern "C" status_t
init_hardware(void)
{
	sDeviceNames[0] = NULL;

	status_t status = get_module(B_PCI_MODULE_NAME, (module_info **)&sPCI);
	if (status != B_OK)
		return status;

	status = probe_devices();
	put_module(B_PCI_MODULE_NAME);
	sPCI = NULL;

	return status;
}


extern "C" const char **
publish_devices(void)
{
	return sDeviceNames;
}


extern "C" device_hooks *
find_device(const char *name)
{
	if (find_gpu_index(name) < 0)
		return NULL;
	return &sDeviceHooks;
}


extern "C" status_t
init_driver(void)
{
	return get_module(B_PCI_MODULE_NAME, (module_info **)&sPCI);
}


extern "C" void
uninit_driver(void)
{
	for (int32 i = 0; i < sGpuCount; i++)
		unmap_gpu(&sGpus[i]);

	if (sPCI != NULL) {
		put_module(B_PCI_MODULE_NAME);
		sPCI = NULL;
	}
}


static status_t
open_hook(const char *name, uint32 flags, void **cookie)
{
	(void)flags;
	int32 index = find_gpu_index(name);
	if (index < 0)
		return B_NAME_NOT_FOUND;

	RadeonLegacyGpu *gpu = &sGpus[index];
	status_t status = map_gpu(gpu);
	if (status != B_OK)
		return status;

	gpu->openCount++;
	*cookie = gpu;
	return B_OK;
}


static status_t
close_hook(void *cookie)
{
	RadeonLegacyGpu *gpu = (RadeonLegacyGpu *)cookie;
	if (gpu == NULL)
		return B_BAD_VALUE;
	if (gpu->openCount > 0)
		gpu->openCount--;
	return B_OK;
}


static status_t
free_hook(void *cookie)
{
	return close_hook(cookie);
}


static status_t
read_hook(void *cookie, off_t pos, void *buffer, size_t *length)
{
	(void)cookie; (void)pos; (void)buffer; (void)length;
	return B_NOT_SUPPORTED;
}


static status_t
write_hook(void *cookie, off_t pos, const void *buffer, size_t *length)
{
	(void)cookie; (void)pos; (void)buffer; (void)length;
	return B_NOT_SUPPORTED;
}


static status_t
get_info(RadeonLegacyGpu *gpu, rg_kernel_info *info)
{
	memset(info, 0, sizeof(*info));
	info->vendor_id = gpu->pci.vendor_id;
	info->device_id = gpu->pci.device_id;
	info->revision = gpu->pci.revision;
	info->host_coherent = (gpu->asic != NULL && gpu->asic->is_apu) ? 1 : 0;
	info->mmio_phys = gpu->mmioPhys;
	info->mmio_size = gpu->mmioMapSize;
	info->vram_phys = gpu->fbPhys;
	info->vram_size = gpu->vramSize;
	info->gart_base = 0;
	info->gart_size = 0;
	info->pci_bus = gpu->pci.bus;
	info->pci_device = gpu->pci.device;
	info->pci_function = gpu->pci.function;
	info->irq = gpu->irq;
	return B_OK;
}


static status_t
translate_phys(RadeonLegacyGpu *gpu, rg_translate_request *request)
{
	request->n_ranges = 0;

	if (request->area == gpu->mmioArea) {
		if (request->offset + request->length > gpu->mmioMapSize)
			return B_BAD_VALUE;
		request->ranges[0].phys = gpu->mmioPhys + request->offset;
		request->ranges[0].length = request->length;
		request->n_ranges = 1;
		return B_OK;
	}

	if (request->area == gpu->fbArea) {
		if (request->offset + request->length > gpu->fbMapSize)
			return B_BAD_VALUE;
		request->ranges[0].phys = gpu->fbPhys + request->offset;
		request->ranges[0].length = request->length;
		request->n_ranges = 1;
		return B_OK;
	}

	if (request->address != 0 && request->length > 0) {
		int32 count = get_memory_map(
			(const void *)(uintptr_t)(request->address + request->offset),
			request->length, (physical_entry *)request->ranges,
			RG_TRANSLATE_MAX_RANGES);
		if (count < 0)
			return (status_t)count;
		request->n_ranges = (uint32)count;
		return B_OK;
	}

	return B_BAD_VALUE;
}


static status_t
pci_config(RadeonLegacyGpu *gpu, rg_pci_cfg_request *request)
{
	if (sPCI == NULL)
		return B_NO_INIT;
	if (request->offset >= 256 || request->length > 256 - request->offset)
		return B_BAD_VALUE;
	if (request->length != 1 && request->length != 2 && request->length != 4)
		return B_BAD_VALUE;
	if (request->length > sizeof(request->data))
		return B_BAD_VALUE;

	if (request->write) {
		uint32 value = 0;
		memcpy(&value, request->data, request->length);
		sPCI->write_pci_config(gpu->pci.bus, gpu->pci.device,
			gpu->pci.function, request->offset, request->length, value);
	} else {
		uint32 value = sPCI->read_pci_config(gpu->pci.bus, gpu->pci.device,
			gpu->pci.function, request->offset, request->length);
		memcpy(request->data, &value, request->length);
	}
	return B_OK;
}


/*
 * Bootstrap display contract. There is exactly one CRTC/connector so that the
 * server and the accelerant can exercise the full pipeline. Replacing these
 * three functions with AtomBIOS/DDC calls is the remaining hardware work.
 */
static status_t
get_connectors(RadeonLegacyGpu *gpu, rg_get_connectors *connectors)
{
	(void)gpu;
	memset(connectors, 0, sizeof(*connectors));
	connectors->count = 1;

	rg_connector_info &connector = connectors->connectors[0];
	connector.connector_id = 0;
	connector.connector_type = DRM_MODE_CONNECTOR_Unknown;
	connector.crtc_id = 0;
	connector.connected = true;
	connector.active = true;
	connector.encoder_id = 0;
	connector.dpms_mode = DRM_MODE_DPMS_ON;
	connector.edid_size = 0;
	return B_OK;
}


static status_t
get_crtcs(RadeonLegacyGpu *gpu, rg_get_crtcs *crtcs)
{
	(void)gpu;
	memset(crtcs, 0, sizeof(*crtcs));
	crtcs->count = 1;

	rg_crtc_info &crtc = crtcs->crtcs[0];
	crtc.crtc_id = 0;
	crtc.active = true;
	crtc.x = 0;
	crtc.y = 0;
	crtc.width = 1024;
	crtc.height = 768;
	crtc.fb_id = 0;
	crtc.gamma_size = 0;
	return B_OK;
}


static status_t
set_crtc(RadeonLegacyGpu *gpu, rg_set_crtc *crtc)
{
	(void)gpu;
	if (crtc->crtc_id != 0)
		return B_BAD_VALUE;
	dprintf("radeon_legacy: %s: modesetting requires the AtomBIOS backend\n",
		gpu->name);
	return B_NOT_SUPPORTED;
}


static status_t
page_flip(RadeonLegacyGpu *gpu, rg_page_flip *flip)
{
	(void)flip;
	dprintf("radeon_legacy: %s: page flip requires the AtomBIOS backend\n",
		gpu->name);
	return B_NOT_SUPPORTED;
}


static status_t
hotplug_event(RadeonLegacyGpu *gpu, rg_hotplug_event *event)
{
	(void)gpu;
	memset(event, 0, sizeof(*event));
	return B_NOT_SUPPORTED;
}


static status_t
control_hook(void *cookie, uint32 op, void *buffer, size_t length)
{
	RadeonLegacyGpu *gpu = (RadeonLegacyGpu *)cookie;
	if (gpu == NULL)
		return B_BAD_VALUE;

	switch (op) {
		case B_GET_ACCELERANT_SIGNATURE:
			if (buffer == NULL || length < sizeof(RG_LEGACY_ACCELERANT_NAME))
				return B_BAD_VALUE;
			strcpy((char *)buffer, RG_LEGACY_ACCELERANT_NAME);
			return B_OK;

		case RG_IOCTL_GET_INFO:
			if (buffer == NULL || length < sizeof(rg_kernel_info))
				return B_BAD_VALUE;
			return get_info(gpu, (rg_kernel_info *)buffer);

		case RG_IOCTL_MAP_MMIO:
			if (buffer == NULL || length < sizeof(rg_map_request))
				return B_BAD_VALUE;
			((rg_map_request *)buffer)->area = gpu->mmioArea;
			return B_OK;

		case RG_IOCTL_MAP_VRAM:
			if (buffer == NULL || length < sizeof(rg_map_request))
				return B_BAD_VALUE;
			((rg_map_request *)buffer)->area = gpu->fbArea;
			return B_OK;

		case RG_IOCTL_TRANSLATE_PHYS:
			if (buffer == NULL || length < sizeof(rg_translate_request))
				return B_BAD_VALUE;
			return translate_phys(gpu, (rg_translate_request *)buffer);

		case RG_IOCTL_IRQ_ATTACH: {
			if (buffer == NULL || length < sizeof(rg_irq_request))
				return B_BAD_VALUE;
			rg_irq_request *request = (rg_irq_request *)buffer;
			if (gpu->irqAttached)
				return B_BUSY;
			gpu->irqSem = request->sem;
			gpu->irqStatusReg = request->status_reg;
			gpu->irqMask = request->mask;
			status_t status = install_io_interrupt_handler(gpu->irq,
				irq_handler, gpu, 0);
			if (status != B_OK) {
				gpu->irqSem = -1;
				gpu->irqStatusReg = 0;
				gpu->irqMask = 0;
				return status;
			}
			gpu->irqAttached = true;
			return B_OK;
		}

		case RG_IOCTL_IRQ_DETACH:
			if (!gpu->irqAttached)
				return B_BAD_VALUE;
			remove_io_interrupt_handler(gpu->irq, irq_handler, gpu);
			gpu->irqAttached = false;
			gpu->irqSem = -1;
			gpu->irqStatusReg = 0;
			gpu->irqMask = 0;
			return B_OK;

		case RG_IOCTL_PCI_CFG:
			if (buffer == NULL || length < sizeof(rg_pci_cfg_request))
				return B_BAD_VALUE;
			return pci_config(gpu, (rg_pci_cfg_request *)buffer);

		case RG_IOCTL_GET_CONNECTORS:
			if (buffer == NULL || length < sizeof(rg_get_connectors))
				return B_BAD_VALUE;
			return get_connectors(gpu, (rg_get_connectors *)buffer);

		case RG_IOCTL_GET_CRTCS:
			if (buffer == NULL || length < sizeof(rg_get_crtcs))
				return B_BAD_VALUE;
			return get_crtcs(gpu, (rg_get_crtcs *)buffer);

		case RG_IOCTL_SET_CRTC:
			if (buffer == NULL || length < sizeof(rg_set_crtc))
				return B_BAD_VALUE;
			return set_crtc(gpu, (rg_set_crtc *)buffer);

		case RG_IOCTL_PAGE_FLIP:
			if (buffer == NULL || length < sizeof(rg_page_flip))
				return B_BAD_VALUE;
			return page_flip(gpu, (rg_page_flip *)buffer);

		case RG_IOCTL_HOTPLUG_EVENT:
			if (buffer == NULL || length < sizeof(rg_hotplug_event))
				return B_BAD_VALUE;
			return hotplug_event(gpu, (rg_hotplug_event *)buffer);

		case RG_IOCTL_BPORT_TRANSPORT:
			/* On real Haiku the server talks to the helper over BPorts. */
			return B_NOT_SUPPORTED;

		default:
			return B_BAD_VALUE;
	}
}
