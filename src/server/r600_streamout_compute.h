/*
 * r600_streamout_compute.h - Emulación nativa de Compute Shaders en R600 vía Transform Feedback (Stream-Out)
 * Permite ejecutar cargas de cómputo en la ALU VLIW5 sin recaer en el CPU.
 */
#ifndef R600_STREAMOUT_COMPUTE_H
#define R600_STREAMOUT_COMPUTE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct r600_compute_pipeline {
	uint32_t workgroup_size[3];
	const uint8_t *spirv_bytecode;
	size_t bytecode_size;
};

/* Traduce y empaqueta un kernel de cómputo para ejecución geométrica en R600 */
bool r600_compile_compute_to_streamout(const struct r600_compute_pipeline *pipeline, uint32_t *out_pm4_buffer, uint32_t *out_dwords);

#ifdef __cplusplus
}
#endif

#endif /* R600_STREAMOUT_COMPUTE_H */
