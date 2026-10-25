/*
 * amdgpu_shim.h - Capa de ioctls amdgpu_drm.h (C9 / §7A.6).
 */
#ifndef RG_AMDGPU_SHIM_H
#define RG_AMDGPU_SHIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t area_id;

/* Debe devolver "amdgpu" (§7A.6). */
const char *amdgpu_shim_name(void);

/* Informacion de dispositivo ampliada (CU count, GFX level, limits). */
struct amdgpu_dev_info {
	uint32_t family;
	uint32_t gfx_level;
	uint32_t cu_count;
	uint32_t max_se;
	uint64_t va_max;
	uint32_t has_vm;
	uint32_t has_sdma;
	uint32_t has_mec;
	uint32_t has_psp;
	uint32_t has_smu;
};

int amdgpu_shim_dev_info(struct amdgpu_dev_info *out);

/* VA op: map/unmap en el GPUVM del cliente. */
struct amdgpu_va_op {
	uint64_t offset;   /* VA */
	uint64_t addr;     /* PA */
	uint64_t size;
	uint32_t op;       /* 0=map, 1=unmap */
	uint32_t flags;    /* RG_VM_* */
	uint32_t handle;
	uint32_t pad;
};

int amdgpu_shim_va_op(struct amdgpu_va_op *op);

/* Timeline syncobj (§7A.6). */
struct amdgpu_timeline {
	uint64_t point;
	uint64_t value;
	uint32_t handle;
	uint32_t pad;
};

int amdgpu_shim_timeline_signal(struct amdgpu_timeline *t);
int amdgpu_shim_timeline_wait(struct amdgpu_timeline *t, uint32_t timeout_ms);
int amdgpu_shim_timeline_transfer(struct amdgpu_timeline *t);

#ifdef __cplusplus
}
#endif

#endif /* RG_AMDGPU_SHIM_H */
