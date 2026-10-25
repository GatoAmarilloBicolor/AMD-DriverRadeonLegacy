/*
 * lvp_planner.h - Planificador hibrido GPU+CPU (C5 / §8).
 */
#ifndef RG_LVP_PLANNER_H
#define RG_LVP_PLANNER_H

#include <stdint.h>
#include "../common/rg_types.h"

enum rg_vk_op {
	RG_OP_RENDER = 0,
	RG_OP_SAMPLER,
	RG_OP_VS_FS,
	RG_OP_GEOMETRY,
	RG_OP_COMPUTE,
	RG_OP_SSBO,
	RG_OP_COPY,
	RG_OP_OCCLUSION,
	RG_OP_COUNT
};

enum rg_executor {
	RG_EXEC_GPU = 0,
	RG_EXEC_CPU = 1
};

struct rg_planner_input {
	rg_family family;
	rg_vk_op op;
	uint32_t shared_size;   /* compute: LDS */
	int force_cpu;          /* RG_VK_MODE=cpu-only */
};

struct rg_planner_decision {
	rg_executor executor;
	const char *reason;
};

rg_planner_decision rg_planner_decide(const rg_planner_input &in);

/* §8.5: coherencia de recursos compartidos */
struct rg_resource {
	uint64_t gpu_address;
	void *host_ptr;
	uint64_t size;
	bool is_coherent;
	bool is_vram_local;
};

/* Flush/invalidate de cache segun plataforma (K5 en ARM). */
void rg_planner_sync_resource(rg_resource *res, bool writing_from_cpu);

#endif /* RG_LVP_PLANNER_H */
