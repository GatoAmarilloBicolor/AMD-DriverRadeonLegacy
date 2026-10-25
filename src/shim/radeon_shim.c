/*
 * radeon_shim.c - libdrm_haiku (C3 / §6.2, ADR-002).
 *
 * Emula el subconjunto de radeon_drm.h que usa el winsys de Mesa. Los
 * numeros de ioctl y layouts deben coincidir con la UAPI MIT de libdrm
 * ([VERIFICAR] contra radeon_drm.h de la version de Mesa usada).
 */
#include "radeon_shim.h"
#include "../common/rg_types.h"
#include "../common/rg_ipc.h"

#include <string.h>
#include <errno.h>

/* --- ioctls (provisionales, verificar en radeon_drm.h) ------------- */
enum {
	DRM_RADEON_INFO = 0x00,
	DRM_RADEON_GEM_CREATE = 0x01,
	DRM_RADEON_GEM_MMAP = 0x02,
	DRM_RADEON_GEM_WAIT_IDLE = 0x03,
	DRM_RADEON_GEM_BUSY = 0x04,
	DRM_RADEON_GEM_SET_TILING = 0x05,
	DRM_RADEON_GEM_GET_TILING = 0x06,
	DRM_RADEON_GEM_CLOSE = 0x07,
	DRM_RADEON_CS = 0x08
};

struct drm_radeon_gem_create {
	uint64_t size;
	uint64_t alignment;
	uint32_t initial_domain;
	uint32_t flags;
	uint32_t handle;
};

struct drm_radeon_gem_mmap {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
	uint64_t size;
	uint64_t addr_ptr;
};

struct drm_radeon_gem_wait_idle {
	uint32_t handle;
	uint32_t pad;
	uint64_t timeout;
};

struct drm_radeon_gem_busy {
	uint32_t handle;
	uint32_t domain;
};

struct drm_radeon_gem_set_tiling {
	uint32_t handle;
	uint32_t tiling_flags;
};

struct drm_radeon_gem_get_tiling {
	uint32_t handle;
	uint32_t tiling_flags;
};

struct drm_radeon_gem_close {
	uint32_t handle;
	uint32_t pad;
};

struct drm_radeon_info {
	uint32_t request;
	uint32_t value;
};

/* --- transporte --------------------------------------------------- */
static rg_transport_fn g_transport;
static uint32_t g_team = 1;
static int g_next_fd = 100;

void rg_shim_set_transport(rg_transport_fn fn) { g_transport = fn; }
void rg_shim_set_team(uint32_t team) { g_team = team; }

int rg_shim_open(void) { return g_next_fd++; }

int rg_shim_close(int fd)
{
	(void)fd;
	return 0;
}

int rg_shim_team_for_fd(int fd)
{
	(void)fd;
	return (int)g_team;
}

static int32_t call(uint32_t msg, const void *payload, uint32_t payload_size,
	void *reply, uint32_t reply_size)
{
	if (g_transport == NULL)
		return RG_ENODEV;
	return g_transport(g_team, msg, payload, payload_size, reply, reply_size);
}

/* --- ioctls ------------------------------------------------------- */
static int shim_gem_create(struct drm_radeon_gem_create *args)
{
	rg_bo_create req;
	memset(&req, 0, sizeof(req));
	req.size = args->size;
	req.alignment = (uint32_t)args->alignment;
	req.domains = args->initial_domain;
	req.flags = args->flags;
	rg_bo_reply rep;
	memset(&rep, 0, sizeof(rep));
	int32_t st = call(RG_BO_CREATE, &req, sizeof(req), &rep, sizeof(rep));
	if (st < 0)
		return (int)st;
	if (rep.status != 0)
		return (int)rep.status;
	args->handle = rep.handle;
	return 0;
}

static int shim_gem_mmap(struct drm_radeon_gem_mmap *args)
{
	/* En Haiku: clone_area del area_id que devolvio el servidor. En el host
	 * devolvemos la direccion del BO mapeado. */
	uint32_t handle = args->handle;
	void *ptr = NULL;
	int32_t st = call(RG_BO_MMAP, &handle, sizeof(handle), &ptr, sizeof(ptr));
	if (st < 0)
		return (int)st;
	if (ptr == NULL)
		return -EINVAL;
	args->addr_ptr = (uint64_t)(uintptr_t)ptr;
	return 0;
}

static int shim_gem_wait_idle(struct drm_radeon_gem_wait_idle *args)
{
	uint32_t handle = args->handle;
	int32_t st = 0;
	int32_t r = call(RG_BO_WAIT_IDLE, &handle, sizeof(handle), &st, sizeof(st));
	if (r < 0)
		return (int)r;
	return st;
}

static int shim_gem_busy(struct drm_radeon_gem_busy *args)
{
	uint32_t handle = args->handle;
	uint32_t busy = 0;
	int32_t r = call(RG_BO_BUSY, &handle, sizeof(handle), &busy, sizeof(busy));
	if (r < 0)
		return (int)r;
	args->domain = busy ? RG_DOMAIN_GTT : 0;
	return 0;
}

static int shim_gem_set_tiling(struct drm_radeon_gem_set_tiling *args)
{
	rg_bo_tiling t;
	t.handle = args->handle;
	t.tiling = args->tiling_flags;
	int32_t st = 0;
	int32_t r = call(RG_BO_SET_TILING, &t, sizeof(t), &st, sizeof(st));
	if (r < 0)
		return (int)r;
	return st;
}

static int shim_gem_get_tiling(struct drm_radeon_gem_get_tiling *args)
{
	uint32_t handle = args->handle;
	uint32_t tiling = 0;
	int32_t r = call(RG_BO_GET_TILING, &handle, sizeof(handle), &tiling, sizeof(tiling));
	if (r < 0)
		return (int)r;
	args->tiling_flags = tiling;
	return 0;
}

static int shim_gem_close(struct drm_radeon_gem_close *args)
{
	uint32_t handle = args->handle;
	int32_t st = 0;
	int32_t r = call(RG_BO_CLOSE, &handle, sizeof(handle), &st, sizeof(st));
	if (r < 0)
		return (int)r;
	return st;
}

static int shim_info(struct drm_radeon_info *args)
{
	rg_info_reply info;
	memset(&info, 0, sizeof(info));
	int32_t r = call(RG_INFO, NULL, 0, &info, sizeof(info));
	if (r < 0)
		return (int)r;
	/* Solo los RADEON_INFO usados al arrancar. */
	switch (args->request) {
		case 0x01: args->value = info.device_id; break;   /* DEVICE_ID */
		case 0x02: args->value = info.family; break;      /* FAMILY */
		case 0x03: args->value = 1; break;                /* ACCEL_WORKING */
		case 0x04: args->value = info.vram_size ? 1 : 0; break;
		default: return -EINVAL;
	}
	return 0;
}

int drmCommandWriteRead(int fd, unsigned long request, void *data, unsigned long size)
{
	(void)fd; (void)size;
	if (data == NULL)
		return -EINVAL;
	switch (request) {
		case DRM_RADEON_INFO: return shim_info((struct drm_radeon_info *)data);
		case DRM_RADEON_GEM_CREATE: return shim_gem_create((struct drm_radeon_gem_create *)data);
		case DRM_RADEON_GEM_MMAP: return shim_gem_mmap((struct drm_radeon_gem_mmap *)data);
		case DRM_RADEON_GEM_WAIT_IDLE: return shim_gem_wait_idle((struct drm_radeon_gem_wait_idle *)data);
		case DRM_RADEON_GEM_BUSY: return shim_gem_busy((struct drm_radeon_gem_busy *)data);
		case DRM_RADEON_GEM_SET_TILING: return shim_gem_set_tiling((struct drm_radeon_gem_set_tiling *)data);
		case DRM_RADEON_GEM_GET_TILING: return shim_gem_get_tiling((struct drm_radeon_gem_get_tiling *)data);
		case DRM_RADEON_GEM_CLOSE: return shim_gem_close((struct drm_radeon_gem_close *)data);
		default: return -EINVAL;
	}
}

int drmIoctl(int fd, unsigned long request, void *arg)
{
	return drmCommandWriteRead(fd, request, arg, 0);
}

void *drmMmap(int fd, void *addr, size_t length, int prot, int flags, int64_t offset)
{
	(void)fd; (void)addr; (void)length; (void)prot; (void)flags; (void)offset;
	/* En Haiku: clone_area / map_bo. En el host, el winsys ya obtuvo el
	 * puntero por GEM_MMAP. */
	return NULL;
}
