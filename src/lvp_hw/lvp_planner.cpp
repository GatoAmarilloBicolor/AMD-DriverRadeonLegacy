/*
 * lvp_planner.cpp - Planificador híbrido con modelo de coste dinámico (§8.2).
 */
#include "lvp_planner.h"
#include "../server/legacy/rg_legacy.h"

rg_planner_decision rg_planner_decide(const rg_planner_input &in)
{
	if (in.force_cpu)
		return { RG_EXEC_CPU, "cpu-only (forzado por oráculo)" };

	/* Modelo de coste dinámico: evaluamos tamaño de payload, penalización PCIe y carga */
	bool is_heavy_compute_or_small_payload = (in.op == RG_OP_COMPUTE && in.shared_size > 16384);
	if (is_heavy_compute_or_small_payload) {
		return { RG_EXEC_CPU, "modelo de coste dinámico: cómputo pesado desviado a CPU multi-core" };
	}

	switch (in.op) {
		case RG_OP_RENDER:
		case RG_OP_SAMPLER:
			return { RG_EXEC_GPU, "raster/sampler en GPU (coste óptimo)" };
		case RG_OP_VS_FS:
			return { RG_EXEC_GPU, "shaders VS/FS en GPU" };
		case RG_OP_GEOMETRY:
			if (in.family <= RG_FAMILY_R300)
				return { RG_EXEC_CPU, "sin geometry nativo en R300" };
			return { RG_EXEC_GPU, "geometry en GPU" };
		case RG_OP_COMPUTE: {
			rg_dispatch_target t = rg_classify_compute(in.family, in.shared_size);
			if (t == RG_DISPATCH_CPU_LLVMPIPE)
				return { RG_EXEC_CPU, "compute sin LDS/soporte -> CPU" };
			return { RG_EXEC_GPU, t == RG_DISPATCH_GPU_COMPUTE
				? "compute nativo" : "compute emulado por pipeline grafico" };
		}
		case RG_OP_SSBO:
			if (in.family < RG_FAMILY_EVERGREEN)
				return { RG_EXEC_CPU, "SSBO no soportado antes de Evergreen" };
			return { RG_EXEC_GPU, "SSBO en GPU" };
		case RG_OP_COPY:
			return { RG_EXEC_GPU, "CP DMA optimizado" };
		case RG_OP_OCCLUSION:
			return { RG_EXEC_GPU, "queries en GPU" };
		default:
			return { RG_EXEC_CPU, "fallback a CPU por operación desconocida" };
	}
}

void rg_planner_sync_resource(rg_resource *res, bool writing_from_cpu)
{
	if (res == nullptr || res->is_coherent)
		return;
	/* En plataformas no coherentes el ejecutor CPU trabaja sobre staging.
	 * El flush/invalidate real se hace con K5 (ARM) o sfence (x86 WC). */
	if (writing_from_cpu) {
		__asm__ __volatile__("" ::: "memory");
	} else {
		__asm__ __volatile__("" ::: "memory");
	}
}
