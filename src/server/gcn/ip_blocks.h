/*
 * ip_blocks.h - Ciclo de vida de bloques IP (C9 / §7A.4).
 *
 * Misma estructura que amdgpu: el dispositivo contiene bloques y estos se
 * inicializan en orden fijo. Es la base para que legacy y amdgpu compartan
 * reset/suspend/resume.
 */
#ifndef RG_IP_BLOCKS_H
#define RG_IP_BLOCKS_H

#include <stdint.h>
#include <vector>

#include "../../common/rg_types.h"

struct rg_device;

enum rg_ip_type {
	RG_IP_COMMON = 0,
	RG_IP_NBIO,
	RG_IP_GMC,
	RG_IP_IH,
	RG_IP_PSP,
	RG_IP_SMU,
	RG_IP_GFX,
	RG_IP_SDMA,
	RG_IP_DCE,
	RG_IP_VCN,
	RG_IP_TYPE_COUNT
};

struct rg_ip_block {
	rg_ip_type type;
	const char *name;
	int (*early_init)(rg_device *adev);
	int (*sw_init)(rg_device *adev);
	int (*hw_init)(rg_device *adev);
	int (*late_init)(rg_device *adev);
	int (*suspend)(rg_device *adev);
	int (*resume)(rg_device *adev);
	int (*hw_fini)(rg_device *adev);
	int (*sw_fini)(rg_device *adev);
	bool (*is_idle)(rg_device *adev);
	int (*soft_reset)(rg_device *adev);
};

struct rg_device {
	uint32_t device_id;
	rg_family family;
	uint32_t gfx_level;
	int is_apu;
	const rg_ip_block **blocks;
	uint32_t n_blocks;
	int initialized;
};

/* Tablas por familia (punteros estaticos). */
const rg_ip_block **rg_ip_blocks_for(rg_family family, uint32_t *count);

/* Orden de arranque: COMMON/NBIO -> GMC -> IH -> PSP -> SMU -> GFX -> SDMA */
int rg_device_init(rg_device *adev);
int rg_device_fini(rg_device *adev);
int rg_device_suspend(rg_device *adev);
int rg_device_resume(rg_device *adev);
int rg_device_soft_reset(rg_device *adev);
bool rg_device_is_idle(rg_device *adev);

/* Diagnostico: nombre del bloque. */
const char *rg_ip_type_name(rg_ip_type type);

#endif /* RG_IP_BLOCKS_H */
