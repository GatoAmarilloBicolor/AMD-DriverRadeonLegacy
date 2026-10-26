/*
 * radeon_legacy_driver.cpp - Haiku accelerant for legacy Radeon GPUs.
 *
 * Loaded by app_server as add-on "radeon_legacy.accelerant". It talks to the
 * kernel module exclusively through the RG_IOCTL_* contract in
 * src/helper/helper_ioctls.h, so it works with either the standalone module
 * (src/kernel) or the native radeon_hd helper.
 */
#include <Accelerant.h>
#include <graphic_driver.h>
#include <OS.h>
#include <SupportDefs.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../common/rg_types.h"
#include "../helper/helper_ioctls.h"
#include "../server/asic/rg_asic.h"

/* The ioctl contract must stay byte-identical to the Accelerant types. */
static_assert(sizeof(display_timing) == sizeof(rg_display_timing),
	"display_timing ABI drift");
static_assert(sizeof(display_mode) == sizeof(rg_display_mode),
	"display_mode ABI drift");
static_assert(offsetof(display_mode, timing) == offsetof(rg_display_mode, timing),
	"display_mode.timing ABI drift");
static_assert(offsetof(display_mode, flags) == offsetof(rg_display_mode, flags),
	"display_mode.flags ABI drift");


struct RadeonLegacyState {
	int				fd;
	bool			ownsFD;
	char			path[B_PATH_NAME_LENGTH];
	rg_kernel_info	info;

	area_id			fbArea;
	uint8			*fb;
	uint32			bytesPerRow;

	area_id			modeArea;
	display_mode	*modes;
	uint32			modeCount;

	display_mode	current;
};

static RadeonLegacyState *sState = NULL;


static const display_mode kBootModes[] = {
	/* 640x480@60 */
	{ { 25175, 640, 656, 752, 800, 480, 490, 492, 525, 0 },
		B_RGB32, 640, 480, 0, 0, B_SCROLL },
	/* 800x600@60 */
	{ { 40000, 800, 840, 968, 1056, 600, 601, 605, 628,
		B_POSITIVE_HSYNC | B_POSITIVE_VSYNC },
		B_RGB32, 800, 600, 0, 0, B_SCROLL },
	/* 1024x768@60 */
	{ { 65000, 1024, 1048, 1184, 1344, 768, 771, 777, 806, 0 },
		B_RGB32, 1024, 768, 0, 0, B_SCROLL },
	/* 1280x1024@60 */
	{ { 108000, 1280, 1328, 1440, 1688, 1024, 1025, 1028, 1066,
		B_POSITIVE_HSYNC | B_POSITIVE_VSYNC },
		B_RGB32, 1280, 1024, 0, 0, B_SCROLL },
	/* 1600x1200@60 */
	{ { 162000, 1600, 1664, 1856, 2160, 1200, 1201, 1204, 1250,
		B_POSITIVE_HSYNC | B_POSITIVE_VSYNC },
		B_RGB32, 1600, 1200, 0, 0, B_SCROLL }
};

static const uint32 kBootModeCount
	= sizeof(kBootModes) / sizeof(kBootModes[0]);


static status_t
issue_ioctl(int fd, uint32 op, void *argument, size_t size)
{
	if (ioctl(fd, op, argument, size) < 0) {
		if (errno == EINVAL || errno == ENOTTY)
			return B_BAD_VALUE;
		return B_NOT_SUPPORTED;
	}
	return B_OK;
}


static const char *
family_name(uint32_t deviceID, uint32_t revision)
{
	/* Subsystem is unknown here; pass the wildcard. */
	const rg_asic_desc *desc
		= rg_asic_lookup(deviceID, revision, 0xffffffffu);
	if (desc == NULL)
		return "Radeon";
	return rg_asic_family_name(desc->family);
}


static status_t
build_mode_list(RadeonLegacyState *state)
{
	size_t size = kBootModeCount * sizeof(display_mode);
	void *base = NULL;

	state->modeArea = create_area("radeon legacy modes", &base,
		B_ANY_ADDRESS, size, B_NO_LOCK,
		B_READ_AREA | B_WRITE_AREA | B_CLONEABLE_AREA);
	if (state->modeArea < 0)
		return state->modeArea;

	state->modes = (display_mode *)base;
	memcpy(state->modes, kBootModes, size);
	state->modeCount = kBootModeCount;
	return B_OK;
}


static void
pick_current_mode(RadeonLegacyState *state)
{
	rg_get_crtcs crtcs;
	memset(&crtcs, 0, sizeof(crtcs));

	if (issue_ioctl(state->fd, RG_IOCTL_GET_CRTCS, &crtcs, sizeof(crtcs))
			== B_OK && crtcs.count > 0) {
		const rg_crtc_info &crtc = crtcs.crtcs[0];
		for (uint32 i = 0; i < state->modeCount; i++) {
			if (state->modes[i].timing.h_display == crtc.width
				&& state->modes[i].timing.v_display == crtc.height) {
				state->current = state->modes[i];
				state->bytesPerRow = crtc.width * 4;
				return;
			}
		}
	}

	/* 1024x768 is the third bootstrap mode. */
	state->current = state->modes[2];
	state->bytesPerRow = state->current.virtual_width * 4;
}


static void
destroy_state(RadeonLegacyState *state)
{
	if (state == NULL)
		return;

	if (state->modeArea >= 0)
		delete_area(state->modeArea);
	if (state->fbArea >= 0)
		delete_area(state->fbArea);
	if (state->ownsFD && state->fd >= 0)
		close(state->fd);

	free(state);
}


static status_t
rg_init_accelerant(int fd)
{
	if (sState != NULL)
		return B_BUSY;
	if (fd < 0)
		return B_BAD_VALUE;

	RadeonLegacyState *state
		= (RadeonLegacyState *)calloc(1, sizeof(RadeonLegacyState));
	if (state == NULL)
		return B_NO_MEMORY;

	state->fd = fd;
	state->fbArea = -1;
	state->modeArea = -1;

	status_t status = issue_ioctl(fd, RG_IOCTL_GET_INFO, &state->info,
		sizeof(state->info));
	if (status != B_OK)
		goto error;

	snprintf(state->path, sizeof(state->path), "/dev/graphics/"
		"radeon_legacy_%02x%02x%02x", (unsigned)state->info.pci_bus,
		(unsigned)state->info.pci_device, (unsigned)state->info.pci_function);

	rg_map_request map;
	memset(&map, 0, sizeof(map));
	map.area = -1;
	status = issue_ioctl(fd, RG_IOCTL_MAP_VRAM, &map, sizeof(map));
	if (status != B_OK || map.area < 0) {
		status = B_NOT_SUPPORTED;
		goto error;
	}

	state->fbArea = clone_area("radeon legacy framebuffer",
		(void **)&state->fb, B_ANY_ADDRESS, B_READ_AREA | B_WRITE_AREA,
		map.area);
	if (state->fbArea < 0) {
		status = state->fbArea;
		goto error;
	}

	status = build_mode_list(state);
	if (status != B_OK)
		goto error;

	pick_current_mode(state);
	sState = state;
	return B_OK;

error:
	destroy_state(state);
	return status;
}


static void
rg_uninit_accelerant(void)
{
	if (sState == NULL)
		return;

	RadeonLegacyState *state = sState;
	sState = NULL;
	destroy_state(state);
}


static ssize_t
rg_accelerant_clone_info_size(void)
{
	return B_PATH_NAME_LENGTH;
}


static void
rg_get_accelerant_clone_info(void *info)
{
	if (sState != NULL)
		strlcpy((char *)info, sState->path, B_PATH_NAME_LENGTH);
	else
		((char *)info)[0] = '\0';
}


static status_t
rg_clone_accelerant(void *info)
{
	int fd = open((const char *)info, O_RDWR);
	if (fd < 0)
		return B_NAME_NOT_FOUND;

	status_t status = rg_init_accelerant(fd);
	if (status != B_OK) {
		close(fd);
		return status;
	}

	sState->ownsFD = true;
	return B_OK;
}


static status_t
rg_get_accelerant_device_info(accelerant_device_info *info)
{
	if (sState == NULL)
		return B_NO_INIT;

	memset(info, 0, sizeof(*info));
	info->version = B_ACCELERANT_VERSION;
	strlcpy(info->name, "Radeon Legacy", sizeof(info->name));
	strlcpy(info->chipset, family_name(sState->info.device_id,
		sState->info.revision), sizeof(info->chipset));
	snprintf(info->serial_no, sizeof(info->serial_no), "%04x:%04x rev %02x",
		(unsigned)sState->info.vendor_id, (unsigned)sState->info.device_id,
		(unsigned)sState->info.revision);
	info->memory = sState->info.vram_size > 0xffffffffu
		? 0xffffffffu : (uint32)sState->info.vram_size;
	info->dac_speed = 400;
	return B_OK;
}


static uint32
rg_accelerant_mode_count(void)
{
	return sState != NULL ? sState->modeCount : 0;
}


static status_t
rg_get_mode_list(display_mode *list)
{
	if (sState == NULL || list == NULL)
		return B_BAD_VALUE;

	memcpy(list, sState->modes, sState->modeCount * sizeof(display_mode));
	return B_OK;
}


static status_t
rg_propose_display_mode(display_mode *target, display_mode *low,
	display_mode *high)
{
	if (sState == NULL || target == NULL)
		return B_BAD_VALUE;

	uint32 targetPixels = target->timing.h_display * target->timing.v_display;
	const display_mode *best = &sState->modes[0];
	uint32 bestDelta = 0xffffffffu;

	for (uint32 i = 0; i < sState->modeCount; i++) {
		uint32 pixels = sState->modes[i].timing.h_display
			* sState->modes[i].timing.v_display;
		uint32 delta = pixels > targetPixels
			? pixels - targetPixels : targetPixels - pixels;
		if (delta < bestDelta) {
			bestDelta = delta;
			best = &sState->modes[i];
		}
	}

	*target = *best;
	if (low != NULL)
		*low = *best;
	if (high != NULL)
		*high = *best;
	return B_OK;
}


static status_t
rg_set_display_mode(display_mode *modeToSet)
{
	if (sState == NULL || modeToSet == NULL)
		return B_BAD_VALUE;

	rg_set_crtc crtc;
	memset(&crtc, 0, sizeof(crtc));
	crtc.crtc_id = 0;
	crtc.fb_id = 0;
	crtc.width = modeToSet->timing.h_display;
	crtc.height = modeToSet->timing.v_display;
	crtc.mode_valid = 1;
	memcpy(&crtc.mode, modeToSet, sizeof(crtc.mode));

	status_t status = issue_ioctl(sState->fd, RG_IOCTL_SET_CRTC, &crtc,
		sizeof(crtc));
	if (status != B_OK)
		return status;

	sState->current = *modeToSet;
	sState->bytesPerRow = modeToSet->virtual_width * 4;
	return B_OK;
}


static status_t
rg_get_display_mode(display_mode *currentMode)
{
	if (sState == NULL || currentMode == NULL)
		return B_BAD_VALUE;

	*currentMode = sState->current;
	return B_OK;
}


static status_t
rg_get_frame_buffer_config(frame_buffer_config *frameBuffer)
{
	if (sState == NULL || frameBuffer == NULL)
		return B_BAD_VALUE;

	uint32 offset = sState->current.v_display_start * sState->bytesPerRow
		+ sState->current.h_display_start * 4;

	frameBuffer->frame_buffer = sState->fb + offset;
	frameBuffer->frame_buffer_dma
		= (void *)(uintptr_t)(sState->info.vram_phys + offset);
	frameBuffer->bytes_per_row = sState->bytesPerRow;
	return B_OK;
}


static status_t
rg_get_pixel_clock_limits(display_mode *mode, uint32 *low, uint32 *high)
{
	(void)mode;
	if (low == NULL || high == NULL)
		return B_BAD_VALUE;

	*low = 10000;
	/* R300/R420 DACs top out around 400 MHz; R600+ go higher. */
	*high = 400000;
	return B_OK;
}


static status_t
rg_get_timing_constraints(display_timing_constraints *constraints)
{
	if (constraints == NULL)
		return B_BAD_VALUE;

	memset(constraints, 0, sizeof(*constraints));
	constraints->h_res = 8;
	constraints->h_sync_min = 8;
	constraints->h_sync_max = 200;
	constraints->h_blank_min = 8;
	constraints->h_blank_max = 2048;
	constraints->v_res = 1;
	constraints->v_sync_min = 1;
	constraints->v_sync_max = 16;
	constraints->v_blank_min = 1;
	constraints->v_blank_max = 64;
	return B_OK;
}


static void
rg_set_indexed_colors(uint count, uint8 first, const uint8 *colorData,
	uint32 flags)
{
	(void)count; (void)first; (void)colorData; (void)flags;
	/* Only RGB32 modes are exposed; nothing to program. */
}


static status_t
rg_get_preferred_display_mode(display_mode *preferredMode)
{
	if (sState == NULL || preferredMode == NULL)
		return B_BAD_VALUE;

	for (uint32 i = 0; i < sState->modeCount; i++) {
		if (sState->modes[i].timing.h_display
				== sState->current.timing.h_display
			&& sState->modes[i].timing.v_display
				== sState->current.timing.v_display) {
			*preferredMode = sState->modes[i];
			return B_OK;
		}
	}

	*preferredMode = sState->modes[0];
	return B_OK;
}


static uint32
rg_dpms_capabilities(void)
{
	/* No SET_DPMS ioctl exists yet: report no DPMS support. */
	return 0;
}


static uint32
rg_dpms_mode(void)
{
	return B_DPMS_ON;
}


static status_t
rg_set_dpms_mode(uint32 dpmsFlags)
{
	(void)dpmsFlags;
	return B_NOT_SUPPORTED;
}


static status_t
rg_get_edid_info(void *info, uint32 size, uint32 *version)
{
	if (sState == NULL || info == NULL || version == NULL)
		return B_BAD_VALUE;

	rg_get_connectors connectors;
	memset(&connectors, 0, sizeof(connectors));
	status_t status = issue_ioctl(sState->fd, RG_IOCTL_GET_CONNECTORS,
		&connectors, sizeof(connectors));
	if (status != B_OK || connectors.count == 0)
		return B_NOT_SUPPORTED;

	const rg_connector_info &connector = connectors.connectors[0];
	if (connector.edid_size == 0 || connector.edid_size > size)
		return B_NOT_SUPPORTED;

	memcpy(info, connector.edid, connector.edid_size);
	*version = 1;
	return B_OK;
}


static status_t
rg_move_display(uint16 hDisplayStart, uint16 vDisplayStart)
{
	(void)hDisplayStart;
	(void)vDisplayStart;
	return B_NOT_SUPPORTED;
}


static uint32
rg_accelerant_engine_count(void)
{
	/* No 2D/3D command submission yet; app_server will use software. */
	return 0;
}


static status_t
rg_acquire_engine(uint32 capabilities, uint32 maxWait, sync_token *syncToken,
	engine_token **engineToken)
{
	(void)capabilities;
	(void)maxWait;
	(void)syncToken;
	(void)engineToken;
	return B_NOT_SUPPORTED;
}


static status_t
rg_release_engine(engine_token *engineToken, sync_token *syncToken)
{
	(void)engineToken;
	(void)syncToken;
	return B_OK;
}


static void
rg_wait_engine_idle(void)
{
}


static status_t
rg_get_sync_token(engine_token *engineToken, sync_token *syncToken)
{
	(void)engineToken;
	if (syncToken != NULL) {
		memset(syncToken, 0, sizeof(*syncToken));
		syncToken->engine_id = 0;
	}
	return B_OK;
}


static status_t
rg_sync_to_token(sync_token *syncToken)
{
	(void)syncToken;
	return B_OK;
}


extern "C" void *
get_accelerant_hook(uint32 feature, void *data)
{
	(void)data;

	switch (feature) {
		/* general */
		case B_INIT_ACCELERANT:
			return (void *)rg_init_accelerant;
		case B_UNINIT_ACCELERANT:
			return (void *)rg_uninit_accelerant;
		case B_CLONE_ACCELERANT:
			return (void *)rg_clone_accelerant;
		case B_ACCELERANT_CLONE_INFO_SIZE:
			return (void *)rg_accelerant_clone_info_size;
		case B_GET_ACCELERANT_CLONE_INFO:
			return (void *)rg_get_accelerant_clone_info;
		case B_GET_ACCELERANT_DEVICE_INFO:
			return (void *)rg_get_accelerant_device_info;

		/* mode configuration */
		case B_ACCELERANT_MODE_COUNT:
			return (void *)rg_accelerant_mode_count;
		case B_GET_MODE_LIST:
			return (void *)rg_get_mode_list;
		case B_PROPOSE_DISPLAY_MODE:
			return (void *)rg_propose_display_mode;
		case B_GET_PREFERRED_DISPLAY_MODE:
			return (void *)rg_get_preferred_display_mode;
		case B_SET_DISPLAY_MODE:
			return (void *)rg_set_display_mode;
		case B_GET_DISPLAY_MODE:
			return (void *)rg_get_display_mode;
		case B_GET_FRAME_BUFFER_CONFIG:
			return (void *)rg_get_frame_buffer_config;
		case B_GET_PIXEL_CLOCK_LIMITS:
			return (void *)rg_get_pixel_clock_limits;
		case B_GET_TIMING_CONSTRAINTS:
			return (void *)rg_get_timing_constraints;
		case B_MOVE_DISPLAY:
			return (void *)rg_move_display;
		case B_SET_INDEXED_COLORS:
			return (void *)rg_set_indexed_colors;
		case B_GET_EDID_INFO:
			return (void *)rg_get_edid_info;

		/* DPMS */
		case B_DPMS_CAPABILITIES:
			return (void *)rg_dpms_capabilities;
		case B_DPMS_MODE:
			return (void *)rg_dpms_mode;
		case B_SET_DPMS_MODE:
			return (void *)rg_set_dpms_mode;

		/* engine/synchronization */
		case B_ACCELERANT_ENGINE_COUNT:
			return (void *)rg_accelerant_engine_count;
		case B_ACQUIRE_ENGINE:
			return (void *)rg_acquire_engine;
		case B_RELEASE_ENGINE:
			return (void *)rg_release_engine;
		case B_WAIT_ENGINE_IDLE:
			return (void *)rg_wait_engine_idle;
		case B_GET_SYNC_TOKEN:
			return (void *)rg_get_sync_token;
		case B_SYNC_TO_TOKEN:
			return (void *)rg_sync_to_token;

		/* cursor, blit and overlay hooks are intentionally absent. */
		default:
			return NULL;
	}
}
