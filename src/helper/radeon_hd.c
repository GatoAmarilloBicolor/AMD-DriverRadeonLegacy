/*
 * radeon_hd.c - Helper de kernel (C1 / §5).
 *
 * Solo hace lo que no puede hacer userland: detectar la GPU por PCI, mapear
 * MMIO/VRAM, traducir direcciones fisicas para la GART y entregar
 * interrupciones por semaforo. El resto (CP, anillos, verificador, BOs) vive
 * en radeon_gpud.
 *
 * Produccion: este archivo se integra en el radeon_hd nativo de Haiku (que ya
 * posee modesetting/AtomBIOS) o se compila como add-on de desarrollo con el
 * nombre grafico "graphics/radeon_legacy" (ver helper_ioctls.h). En el host de
 * desarrollo compila con un backend simulado para validar sintaxis y tests.
 *
 * APIs de Haiku usadas (verificadas en haiku/haiku):
 *   get_module(B_PCI_MODULE_NAME), pci_module_info.get_nth_pci_info
 *   map_physical_memory, get_memory_map, physical_entry
 *   install_io_interrupt_handler / remove_io_interrupt_handler
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "../common/rg_types.h"
#include "../common/rg_pci_ids.h"
#include "helper_ioctls.h"

#ifdef __HAIKU__
#  include <KernelExport.h>
#  include <Drivers.h>
#  include <PCI.h>
#  include <OS.h>
#  include <module.h>
#  define RG_LOG(fmt, ...) dprintf("radeon_legacy: " fmt "\n", ##__VA_ARGS__)
#else
/* ------------------------------------------------------------------ */
/* Backend simulado para desarrollo/test en el host                    */
/* ------------------------------------------------------------------ */
#include <sys/types.h>

typedef int32_t status_t;
typedef int32_t sem_id;
typedef int32_t area_id;
typedef uint64_t phys_addr_t;
typedef uintptr_t addr_t;
typedef struct { int dummy; } pci_module_info;

typedef struct device_hooks {
	status_t (*open_hook)(const char *name, uint32_t flags, void **cookie);
	status_t (*close_hook)(void *cookie);
	status_t (*free_hook)(void *cookie);
	status_t (*control_hook)(void *cookie, uint32_t op, void *data, size_t len);
	status_t (*read_hook)(void *cookie, off_t pos, void *buf, size_t *len);
	status_t (*write_hook)(void *cookie, off_t pos, const void *buf, size_t *len);
} device_hooks;

#define B_OK                    0
#define B_ERROR                -1
#define B_BAD_VALUE            -2
#define B_NAME_NOT_FOUND       -3
#define B_NO_MEMORY            -4
#define B_NO_INIT              -5
#define B_BUFFER_OVERFLOW      -6
#define B_PAGE_SIZE          4096u

#define B_KERNEL_READ_AREA     (1u << 0)
#define B_KERNEL_WRITE_AREA    (1u << 1)
#define B_READ_AREA            (1u << 2)
#define B_WRITE_AREA           (1u << 3)
#define B_CLONEABLE_AREA       (1u << 4)
#define B_WRITE_COMBINING_MEMORY (1u << 5)
#define B_ANY_KERNEL_ADDRESS   (1u << 6)
#define B_ANY_KERNEL_BLOCK_ADDRESS (1u << 7)

#define RG_LOG(fmt, ...) ((void)0)

static area_id g_fake_area_next = 1;
static uint32_t g_fake_scratch[1024];

static area_id sim_map_physical_memory(const char *name, phys_addr_t phys,
	size_t size, uint32_t flags, uint32_t protection, void **mapped)
{
	(void)name; (void)phys; (void)size; (void)flags; (void)protection;
	*mapped = g_fake_scratch;
	return g_fake_area_next++;
}
#endif /* __HAIKU__ */

#define RG_MAX_AREAS 16

enum rg_area_kind {
	RG_AREA_MMIO = 1,
	RG_AREA_VRAM = 2
};

struct rg_area_record {
	area_id id;
	uint64_t phys;
	uint64_t size;
	void *virt;
	uint32_t kind;
	int used;
};

static struct rg_area_record g_areas[RG_MAX_AREAS];
static rg_kernel_info g_info;
#ifdef __HAIKU__
static pci_module_info *g_pci;
#endif
static volatile uint8_t *g_mmio;
static int g_irq_attached;
static rg_irq_request g_irq_req;

static struct rg_area_record *find_area(area_id id)
{
	for (size_t i = 0; i < RG_MAX_AREAS; i++)
		if (g_areas[i].used && g_areas[i].id == id)
			return &g_areas[i];
	return NULL;
}

static struct rg_area_record *find_kind(uint32_t kind)
{
	for (size_t i = 0; i < RG_MAX_AREAS; i++)
		if (g_areas[i].used && g_areas[i].kind == kind)
			return &g_areas[i];
	return NULL;
}

static struct rg_area_record *alloc_record(void)
{
	for (size_t i = 0; i < RG_MAX_AREAS; i++) {
		if (!g_areas[i].used) {
			memset(&g_areas[i], 0, sizeof(g_areas[i]));
			g_areas[i].used = 1;
			return &g_areas[i];
		}
	}
	return NULL;
}

/* --- Deteccion de hardware --------------------------------------- */

static area_id map_bar(const char *name, uint64_t phys, uint64_t size,
	int write_combining, void **virt)
{
#ifdef __HAIKU__
	uint32_t flags;
	if (write_combining) {
		flags = B_ANY_KERNEL_BLOCK_ADDRESS | B_WRITE_COMBINING_MEMORY;
	} else {
		flags = B_ANY_KERNEL_ADDRESS;
	}
	
	/* Use B_LARGE_MEMORY for VRAM >= 2MB for huge pages (2MB) */
	if (!write_combining && size >= (2ull << 20)) {
		flags |= B_LARGE_MEMORY;
	}
	
	/* Kernel + user + cloneable: el servidor clona el area desde userland. */
	uint32_t protection = B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA
		| B_READ_AREA | B_WRITE_AREA | B_CLONEABLE_AREA;
	
	return map_physical_memory(name, (phys_addr_t)phys, (size_t)size,
		flags, protection, virt);
#else
	(void)write_combining;
	return sim_map_physical_memory(name, (phys_addr_t)phys, (size_t)size,
		0, 0, virt);
#endif
}

static status_t ensure_region(uint32_t kind, area_id *out)
{
	struct rg_area_record *rec = find_kind(kind);
	if (rec != NULL) {
		*out = rec->id;
		return B_OK;
	}

	uint64_t phys;
	uint64_t size;
	const char *name;
	int wc;
	if (kind == RG_AREA_MMIO) {
		phys = g_info.mmio_phys;
		size = g_info.mmio_size;
		name = "radeon_legacy mmio";
		wc = 0;
	} else {
		phys = g_info.vram_phys;
		size = g_info.vram_size;
		name = "radeon_legacy vram";
		wc = 1;
	}
	if (size == 0)
		return B_NAME_NOT_FOUND;

	rec = alloc_record();
	if (rec == NULL)
		return B_NO_MEMORY;

	void *virt = NULL;
	area_id id = map_bar(name, phys, size, wc, &virt);
	if (id < 0) {
		rec->used = 0;
		return id;
	}

	rec->id = id;
	rec->phys = phys;
	rec->size = size;
	rec->virt = virt;
	rec->kind = kind;
	rec->used = 1;

	if (kind == RG_AREA_MMIO)
		g_mmio = (volatile uint8_t *)virt;

	*out = id;
	return B_OK;
}

#ifdef __HAIKU__
static status_t detect_pci(void)
{
	if (g_pci == NULL)
		return B_NO_INIT;

	pci_info info;
	uint32_t index = 0;
	while (g_pci->get_nth_pci_info(index++, &info) == B_OK) {
		if (info.vendor_id != RG_PCI_VENDOR_AMD)
			continue;
		if (!rg_pci_id_is_supported(info.device_id))
			continue;

		struct {
			uint64_t base;
			uint64_t size;
		} bars[6];
		int nbars = 0;

		for (int i = 0; i < 6; i++) {
			uint64_t size = (uint64_t)info.u.h0.base_register_sizes[i];
			if (size == 0)
				continue;
			if (info.u.h0.base_register_flags[i] & PCI_address_space)
				continue; /* BAR de I/O */

			uint64_t base = (uint64_t)info.u.h0.base_registers[i];
			if (i < 5
				&& (info.u.h0.base_register_flags[i] & PCI_address_type)
					== PCI_address_type_64) {
				base |= (uint64_t)info.u.h0.base_registers[i + 1] << 32;
				size |= (uint64_t)info.u.h0.base_register_sizes[i + 1] << 32;
				i++; /* el siguiente BAR es la mitad alta */
			}
			bars[nbars].base = base;
			bars[nbars].size = size;
			nbars++;
		}
		if (nbars == 0)
			continue;

		/* VRAM = BAR de mayor tamano; MMIO = BAR0 si es valido. */
		int vram = -1;
		for (int i = 0; i < nbars; i++)
			if (vram < 0 || bars[i].size > bars[vram].size)
				vram = i;
		int mmio = -1;
		if (bars[0].size >= B_PAGE_SIZE && bars[0].size <= (64ull << 20)
			&& 0 != vram)
			mmio = 0;
		for (int i = 1; i < nbars && mmio < 0; i++) {
			if (i == vram)
				continue;
			if (bars[i].size >= B_PAGE_SIZE
				&& bars[i].size <= (64ull << 20))
				mmio = i;
		}
		if (mmio < 0 || vram < 0 || bars[vram].size < (16ull << 20))
			continue;

		memset(&g_info, 0, sizeof(g_info));
		g_info.vendor_id = info.vendor_id;
		g_info.device_id = info.device_id;
		g_info.revision = info.revision;
#if defined(__x86_64__) || defined(__i386__) || defined(__aarch64__)
		g_info.host_coherent = 1;
#else
		g_info.host_coherent = 0;
#endif
		g_info.mmio_phys = bars[mmio].base;
		g_info.mmio_size = bars[mmio].size;
		g_info.vram_phys = bars[vram].base;
		g_info.vram_size = bars[vram].size;
		/* La ventana GTT la programa el servidor [VERIFICAR §6.8]. */
		g_info.gart_base = 0;
		g_info.gart_size = 0;
		g_info.pci_bus = info.bus;
		g_info.pci_device = info.device;
		g_info.pci_function = info.function;
		g_info.irq = info.u.h0.interrupt_line;
		return B_OK;
	}
	return B_NAME_NOT_FOUND;
}
#else
struct rg_sim_pci_entry {
	uint32_t device;
	uint32_t revision;
	uint64_t mmio_phys;
	uint64_t mmio_size;
	uint64_t vram_phys;
	uint64_t vram_size;
	uint32_t irq;
};

static const struct rg_sim_pci_entry g_known[] = {
	/* R600: HD 4870 (RV770) */
	{ 0x9440, 0x00, 0xF0000000ull, 0x00020000ull, 0x100000000ull, 0x10000000ull, 0x60 },
	/* Evergreen: HD 5870 (Cypress) */
	{ 0x6898, 0x00, 0xF0000000ull, 0x00020000ull, 0x200000000ull, 0x40000000ull, 0x60 },
	/* Northern Islands: HD 6970 (Cayman) */
	{ 0x6718, 0x00, 0xF0000000ull, 0x00020000ull, 0x200000000ull, 0x80000000ull, 0x60 },
	/* Polaris: RX 480 (Ellesmere) */
	{ 0x67DF, 0xC7, 0xF0000000ull, 0x00040000ull, 0x300000000ull, 0x100000000ull, 0x70 },
};

static status_t detect_pci(void)
{
	const size_t n = sizeof(g_known) / sizeof(g_known[0]);
	for (size_t i = 0; i < n; i++) {
		if (!rg_pci_id_is_supported(g_known[i].device))
			continue;
		memset(&g_info, 0, sizeof(g_info));
		g_info.vendor_id = RG_PCI_VENDOR_AMD;
		g_info.device_id = g_known[i].device;
		g_info.revision = g_known[i].revision;
		g_info.host_coherent = 1;
		g_info.mmio_phys = g_known[i].mmio_phys;
		g_info.mmio_size = g_known[i].mmio_size;
		g_info.vram_phys = g_known[i].vram_phys;
		g_info.vram_size = g_known[i].vram_size;
		g_info.gart_base = 0;
		g_info.gart_size = 0;
		g_info.pci_bus = 1;
		g_info.pci_device = 0;
		g_info.pci_function = 0;
		g_info.irq = g_known[i].irq;
		return B_OK;
	}
	return B_NAME_NOT_FOUND;
}
#endif /* __HAIKU__ */

/* --- IRQ --------------------------------------------------------- */
#ifdef __HAIKU__
static int32 rg_irq_handler(void *data)
{
	rg_irq_request *req = (rg_irq_request *)data;
	if (g_mmio == NULL || req == NULL)
		return B_UNHANDLED_INTERRUPT;

	uint32_t status = *(volatile uint32_t *)(g_mmio + req->status_reg);
	if ((status & req->mask) == 0)
		return B_UNHANDLED_INTERRUPT;

	/* El ack/rearm del bloque IH es del servidor (§6.7). */
	release_sem_etc((sem_id)req->sem, 1, B_DO_NOT_RESCHEDULE);
	return B_HANDLED_INTERRUPT;
}
#endif

/* --- ioctls ------------------------------------------------------ */
static status_t rg_ioctl_get_info(void *out, size_t size)
{
	if (out == NULL || size < sizeof(g_info))
		return B_BAD_VALUE;
	memcpy(out, &g_info, sizeof(g_info));
	return B_OK;
}

static status_t rg_ioctl_map(uint32_t kind, rg_map_request *req)
{
	if (req == NULL)
		return B_BAD_VALUE;
	if ((req->flags & ~(RG_MAP_READ | RG_MAP_WRITE | RG_MAP_WC)) != 0)
		return B_BAD_VALUE;
	if ((req->flags & (RG_MAP_READ | RG_MAP_WRITE)) == 0)
		return B_BAD_VALUE;

	area_id id = -1;
	status_t st = ensure_region(kind, &id);
	if (st != B_OK)
		return st;
	req->area = id;
	return B_OK;
}

static status_t translate_area(const struct rg_area_record *rec,
	rg_translate_request *req)
{
	if (req->offset + req->length > rec->size)
		return B_BAD_VALUE;

	uint64_t phys = rec->phys + req->offset;
	uint64_t remaining = req->length;
	uint32_t n = 0;
	while (remaining > 0 && n < RG_TRANSLATE_MAX_RANGES) {
		uint64_t chunk = remaining;
		if (chunk > (1ull << 20))
			chunk = 1ull << 20;
		req->ranges[n].phys = phys;
		req->ranges[n].length = chunk;
		phys += chunk;
		remaining -= chunk;
		n++;
	}
	if (remaining != 0)
		return B_BUFFER_OVERFLOW;
	req->n_ranges = n;
	return B_OK;
}

static status_t rg_ioctl_translate(rg_translate_request *req)
{
	if (req == NULL || req->length == 0)
		return B_BAD_VALUE;

	if (req->area >= 0) {
		const struct rg_area_record *rec = find_area((area_id)req->area);
		if (rec == NULL)
			return B_BAD_VALUE;
		return translate_area(rec, req);
	}

	if (req->address == 0)
		return B_BAD_VALUE;

#ifdef __HAIKU__
	physical_entry table[RG_TRANSLATE_MAX_RANGES];
	status_t st = get_memory_map((const void *)(addr_t)req->address,
		(size_t)req->length, table, RG_TRANSLATE_MAX_RANGES);
	if (st != B_OK)
		return st;

	uint64_t remaining = req->length;
	uint32_t n = 0;
	while (remaining > 0 && n < RG_TRANSLATE_MAX_RANGES) {
		uint64_t chunk = table[n].size;
		if (chunk == 0)
			break;
		if (chunk > remaining)
			chunk = remaining;
		req->ranges[n].phys = table[n].address;
		req->ranges[n].length = chunk;
		remaining -= chunk;
		n++;
	}
	if (remaining != 0)
		return B_BUFFER_OVERFLOW;
	req->n_ranges = n;
	return B_OK;
#else
	/* Simulacion: identidad virtual->fisica, trozos de 1 MiB. */
	uint64_t phys = req->address;
	uint64_t remaining = req->length;
	uint32_t n = 0;
	while (remaining > 0 && n < RG_TRANSLATE_MAX_RANGES) {
		uint64_t chunk = remaining > (1ull << 20) ? (1ull << 20) : remaining;
		req->ranges[n].phys = phys;
		req->ranges[n].length = chunk;
		phys += chunk;
		remaining -= chunk;
		n++;
	}
	if (remaining != 0)
		return B_BUFFER_OVERFLOW;
	req->n_ranges = n;
	return B_OK;
#endif
}

static status_t rg_ioctl_irq(rg_irq_request *req)
{
	if (req == NULL)
		return B_BAD_VALUE;
	if (g_irq_attached)
		return RG_EBUSY;
	if (req->sem < 0)
		return B_BAD_VALUE;
	if ((req->status_reg & 3u) != 0
		|| (uint64_t)req->status_reg + 4 > g_info.mmio_size)
		return B_BAD_VALUE;

	/* El handler necesita el MMIO mapeado en el kernel. */
	area_id id;
	status_t st = ensure_region(RG_AREA_MMIO, &id);
	if (st != B_OK)
		return st;

	g_irq_req = *req;
#ifdef __HAIKU__
	st = install_io_interrupt_handler((int32)g_info.irq, rg_irq_handler,
		&g_irq_req, 0);
	if (st != B_OK) {
		memset(&g_irq_req, 0, sizeof(g_irq_req));
		return st;
	}
#endif
	g_irq_attached = 1;
	return B_OK;
}

static status_t rg_ioctl_irq_detach(void)
{
	if (!g_irq_attached)
		return B_OK;
#ifdef __HAIKU__
	remove_io_interrupt_handler((int32)g_info.irq, rg_irq_handler, &g_irq_req);
#endif
	g_irq_attached = 0;
	memset(&g_irq_req, 0, sizeof(g_irq_req));
	return B_OK;
}

static status_t rg_ioctl_pci_cfg(rg_pci_cfg_request *req)
{
	if (req == NULL || req->length == 0 || req->length > 256)
		return B_BAD_VALUE;
	if (req->offset + req->length > 256)
		return B_BAD_VALUE;
	if (req->write)
		return RG_EACCES; /* v1: config PCI solo lectura */

#ifdef __HAIKU__
	if (g_pci == NULL)
		return B_NO_INIT;
	for (uint32_t i = 0; i < req->length; i++)
		req->data[i] = g_pci->read_pci_config((uint8)g_info.pci_bus,
			(uint8)g_info.pci_device, (uint8)g_info.pci_function,
			(uint16)(req->offset + i), 1);
#else
	memset(req->data, 0, req->length);
	for (uint32_t i = 0; i < req->length; i++) {
		uint16_t off = (uint16_t)(req->offset + i);
		uint8_t v = 0;
		if (off == 0x00) v = RG_PCI_VENDOR_AMD & 0xff;
		else if (off == 0x01) v = (RG_PCI_VENDOR_AMD >> 8) & 0xff;
		else if (off == 0x02) v = g_info.device_id & 0xff;
		else if (off == 0x03) v = (g_info.device_id >> 8) & 0xff;
		else if (off == 0x08) v = g_info.revision;
		else if (off == 0x0e) v = 0x00; /* header type 0 */
		req->data[i] = v;
	}
#endif
	return B_OK;
}

/* ========================================================================
 * Display / connector contract
 *
 * Connector probing (HPD/DDC), mode programming and page flipping belong to
 * the native Haiku radeon_hd driver (ADR-004), which owns the AtomBIOS and
 * I2C context this standalone helper does not have. On Haiku these ioctls
 * therefore fail closed instead of fabricating hardware state. The host
 * backend exposes one simulated head so the ioctl ABI stays testable.
 * ======================================================================== */

status_t rg_ioctl_get_connectors(rg_get_connectors *connectors)
{
	if (connectors == NULL)
		return B_BAD_VALUE;

	memset(connectors, 0, sizeof(*connectors));
#ifdef __HAIKU__
	return B_NOT_SUPPORTED;
#else
	connectors->count = 1;
	connectors->connectors[0].connector_id = 0;
	connectors->connectors[0].connector_type = DRM_MODE_CONNECTOR_Unknown;
	connectors->connectors[0].crtc_id = 0;
	connectors->connectors[0].connected = true;
	connectors->connectors[0].active = true;
	connectors->connectors[0].encoder_id = 0;
	connectors->connectors[0].dpms_mode = DRM_MODE_DPMS_ON;
	return B_OK;
#endif
}

status_t rg_ioctl_get_crtcs(rg_get_crtcs *crtcs)
{
	if (crtcs == NULL)
		return B_BAD_VALUE;

	memset(crtcs, 0, sizeof(*crtcs));
#ifdef __HAIKU__
	return B_NOT_SUPPORTED;
#else
	crtcs->count = 1;
	crtcs->crtcs[0].crtc_id = 0;
	crtcs->crtcs[0].active = true;
	crtcs->crtcs[0].gamma_size = 0;
	return B_OK;
#endif
}

status_t rg_ioctl_set_crtc(rg_set_crtc *crtc)
{
	if (crtc == NULL)
		return B_BAD_VALUE;

	if (crtc->mode_valid
		&& (crtc->mode.timing.h_total == 0 || crtc->mode.timing.v_total == 0))
		return B_BAD_VALUE;

#ifdef __HAIKU__
	return B_NOT_SUPPORTED;
#else
	RG_LOG("CRTC %u -> %ux%u", crtc->crtc_id,
		crtc->mode.virtual_width, crtc->mode.virtual_height);
	return B_OK;
#endif
}

status_t rg_ioctl_page_flip(rg_page_flip *flip)
{
	if (flip == NULL)
		return B_BAD_VALUE;

#ifdef __HAIKU__
	return B_NOT_SUPPORTED;
#else
	RG_LOG("Page flip CRTC %u -> FB %u", flip->crtc_id, flip->fb_id);
	return B_OK;
#endif
}

status_t rg_ioctl_hotplug_event(rg_hotplug_event *event)
{
	if (event == NULL)
		return B_BAD_VALUE;

	event->connected = 0;
#ifdef __HAIKU__
	return B_NOT_SUPPORTED;
#else
	if (event->connector_id != 0)
		return B_BAD_VALUE;
	event->connected = 1;
	return B_OK;
#endif
}

/*
 * Minimum payload size for each ioctl. A zero result means the ioctl takes no
 * buffer (IRQ_DETACH) or is unknown; the dispatch rejects the latter.
 */
static size_t rg_ioctl_payload_size(uint32_t code)
{
	switch (code) {
		case RG_IOCTL_GET_INFO:			return sizeof(rg_kernel_info);
		case RG_IOCTL_MAP_MMIO:
		case RG_IOCTL_MAP_VRAM:			return sizeof(rg_map_request);
		case RG_IOCTL_TRANSLATE_PHYS:	return sizeof(rg_translate_request);
		case RG_IOCTL_IRQ_ATTACH:		return sizeof(rg_irq_request);
		case RG_IOCTL_IRQ_DETACH:		return 0;
		case RG_IOCTL_PCI_CFG:			return sizeof(rg_pci_cfg_request);
		case RG_IOCTL_GET_CONNECTORS:	return sizeof(rg_get_connectors);
		case RG_IOCTL_GET_CRTCS:		return sizeof(rg_get_crtcs);
		case RG_IOCTL_SET_CRTC:			return sizeof(rg_set_crtc);
		case RG_IOCTL_PAGE_FLIP:		return sizeof(rg_page_flip);
		case RG_IOCTL_HOTPLUG_EVENT:	return sizeof(rg_hotplug_event);
		default:						return 0;
	}
}

static status_t rg_control(void *cookie, uint32_t code, void *buffer, size_t size)
{
	size_t needed = rg_ioctl_payload_size(code);

	(void)cookie;
	if (needed != 0 && (buffer == NULL || size < needed))
		return B_BAD_VALUE;
	switch (code) {
		case RG_IOCTL_GET_INFO:
			return rg_ioctl_get_info(buffer, size);
		case RG_IOCTL_MAP_MMIO:
			return rg_ioctl_map(RG_AREA_MMIO, (rg_map_request *)buffer);
		case RG_IOCTL_MAP_VRAM:
			return rg_ioctl_map(RG_AREA_VRAM, (rg_map_request *)buffer);
		case RG_IOCTL_TRANSLATE_PHYS:
			return rg_ioctl_translate((rg_translate_request *)buffer);
		case RG_IOCTL_IRQ_ATTACH:
			return rg_ioctl_irq((rg_irq_request *)buffer);
		case RG_IOCTL_IRQ_DETACH:
			return rg_ioctl_irq_detach();
		case RG_IOCTL_PCI_CFG:
			return rg_ioctl_pci_cfg((rg_pci_cfg_request *)buffer);
		case RG_IOCTL_GET_CONNECTORS:
			return rg_ioctl_get_connectors((rg_get_connectors *)buffer);
		case RG_IOCTL_GET_CRTCS:
			return rg_ioctl_get_crtcs((rg_get_crtcs *)buffer);
		case RG_IOCTL_SET_CRTC:
			return rg_ioctl_set_crtc((rg_set_crtc *)buffer);
		case RG_IOCTL_PAGE_FLIP:
			return rg_ioctl_page_flip((rg_page_flip *)buffer);
		case RG_IOCTL_HOTPLUG_EVENT:
			return rg_ioctl_hotplug_event((rg_hotplug_event *)buffer);
		default:
			return B_BAD_VALUE;
	}
}

static status_t rg_open(const char *name, uint32_t flags, void **cookie)
{
	(void)name; (void)flags;
	if (cookie == NULL)
		return B_BAD_VALUE;
	*cookie = &g_info;
	return B_OK;
}

static status_t rg_close(void *cookie)
{
	(void)cookie;
	return B_OK;
}

static status_t rg_free(void *cookie)
{
	(void)cookie;
	return B_OK;
}

static device_hooks g_device_hooks = {
	.open_hook = rg_open,
	.close_hook = rg_close,
	.free_hook = rg_free,
	.control_hook = rg_control,
};

/* Helper to clone kernel area into userland address space (server clones for clients) */
#ifdef __HAIKU__
static status_t clone_area_for_user(area_id kernel_area, const char *name, void **user_addr)
{
	if (kernel_area < 0)
		return B_BAD_VALUE;
	
	area_id user_area = clone_area(name, user_addr, B_ANY_ADDRESS,
		B_READ_AREA | B_WRITE_AREA, kernel_area);
	if (user_area < 0)
		return user_area;
	
	return B_OK;
}
#else
static status_t clone_area_for_user(area_id kernel_area, const char *name, void **user_addr)
{
	(void)kernel_area; (void)name; (void)user_addr;
	return B_OK;
}
#endif

/* --- entry points ------------------------------------------------ */
status_t init_hardware(void)
{
#ifdef __HAIKU__
	status_t st = get_module(B_PCI_MODULE_NAME, (module_info **)&g_pci);
	if (st != B_OK)
		return st;
#endif
	status_t st = detect_pci();
#ifdef __HAIKU__
	if (st != B_OK) {
		if (g_pci != NULL) {
			put_module(B_PCI_MODULE_NAME);
			g_pci = NULL;
		}
	} else {
		/* Keep PCI module for later use (PCI config reads) */
	}
#endif
	if (st != B_OK) {
		RG_LOG("GPU Radeon soportada no encontrada");
		return st;
	}
	RG_LOG("GPU %04x:%04x rev %02x mmio=%llx:%llx vram=%llx:%llx",
		g_info.vendor_id, g_info.device_id, g_info.revision,
		(unsigned long long)g_info.mmio_phys, (unsigned long long)g_info.mmio_size,
		(unsigned long long)g_info.vram_phys, (unsigned long long)g_info.vram_size);
	return B_OK;
}

status_t init_driver(void)
{
	if (g_info.device_id == 0) {
#ifdef __HAIKU__
		if (g_pci == NULL) {
			status_t st = get_module(B_PCI_MODULE_NAME, (module_info **)&g_pci);
			if (st != B_OK)
				return st;
		}
#endif
		status_t st = detect_pci();
		if (st != B_OK) {
#ifdef __HAIKU__
			if (g_pci != NULL) {
				put_module(B_PCI_MODULE_NAME);
				g_pci = NULL;
			}
#endif
			return st;
		}
	}
	return B_OK;
}

void uninit_driver(void)
{
	if (g_irq_attached)
		rg_ioctl_irq_detach();

#ifdef __HAIKU__
	for (size_t i = 0; i < RG_MAX_AREAS; i++) {
		if (g_areas[i].used)
			delete_area(g_areas[i].id);
	}
	if (g_pci != NULL) {
		put_module(B_PCI_MODULE_NAME);
		g_pci = NULL;
	}
#endif

	memset(g_areas, 0, sizeof(g_areas));
	memset(&g_info, 0, sizeof(g_info));
	g_mmio = NULL;
	RG_LOG("driver descargado");
}

/* Public API for server to clone helper areas into client address space */
status_t rg_helper_clone_area(area_id kernel_area, const char *name, void **user_addr)
{
	return clone_area_for_user(kernel_area, name, user_addr);
}

const char **publish_devices(void)
{
	static const char *devices[] = { RG_HELPER_DEVICE_NAME, NULL };
	return devices;
}

device_hooks *find_device(const char *name)
{
	if (name != NULL && strcmp(name, RG_HELPER_DEVICE_NAME) == 0)
		return &g_device_hooks;
	return NULL;
}
