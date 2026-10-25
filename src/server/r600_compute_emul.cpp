/*
 * r600_compute_emul.cpp - Implementación de la emulación compute en R600/R700
 */
#include "r600_compute_emul.h"

enum r600_dispatch_target r600_classify_and_emulate_dispatch(const struct r600_compute_workload *workload)
{
	if (workload == nullptr)
		return R600_DISPATCH_CPU_SIMD_FALLBACK;
	
	/* R600/R700 carecen de hardware nativo de Compute (CS).
	 * Si el shader usa memoria compartida (LDS) o sincronización compleja entre workgroups,
	 * requiere fallback al oráculo CPU con SIMD (K1-K13).
	 * De lo contrario, se emula en GPU mediante Vertex Shader + Stream-Out (Transform Feedback).
	 */
	if (workload->uses_shared_memory) {
		return R600_DISPATCH_CPU_SIMD_FALLBACK;
	}
	
	return R600_DISPATCH_GPU_GRAPHICS_EMUL;
}
