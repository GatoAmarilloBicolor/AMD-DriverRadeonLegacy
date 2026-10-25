/*
 * r600_compute_emul.h - Emulación de Compute Shader en R600/R700 mediante Pipeline Gráfico (Stream-Out)
 * Basado en §8.14 de PLAN_RADEON_LEGACY_HAIKU-2.md.
 */
#ifndef R600_COMPUTE_EMUL_H
#define R600_COMPUTE_EMUL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Destino del disipador de cómputo para R600/R700 */
enum r600_dispatch_target {
	R600_DISPATCH_GPU_GRAPHICS_EMUL = 0, /* Emulación por Vertex Shader + Stream-Out (§8.14) */
	R600_DISPATCH_CPU_SIMD_FALLBACK      /* Fallback a CPU por uso de LDS / Group Memory */
};

struct r600_compute_workload {
	uint32_t workgroup_size_x;
	uint32_t workgroup_size_y;
	uint32_t num_groups_x;
	uint32_t num_groups_y;
	bool uses_shared_memory;             /* nir->info.shared_size > 0 */
};

/* Clasificador y emulador de compute para R600/R700 */
enum r600_dispatch_target r600_classify_and_emulate_dispatch(const struct r600_compute_workload *workload);

#ifdef __cplusplus
}
#endif

#endif /* R600_COMPUTE_EMUL_H */
