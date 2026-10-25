/*
 * rocm_runtime.h - Entorno de ejecución ROCm / OpenCL de bajo nivel para aceleración de IA en Radeon Legacy
 */
#ifndef ROCM_RUNTIME_H
#define ROCM_RUNTIME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rocm_kernel_args {
	const char *kernel_name;
	uint32_t grid_size[3];
	uint32_t block_size[3];
	void *args_buffer;
	size_t args_size;
};

/* Ejecución de kernel ROCm / OpenCL sobre las unidades de cómputo o oráculo CPU */
bool rocm_runtime_submit_kernel(const struct rocm_kernel_args *args);

#ifdef __cplusplus
}
#endif

#endif /* ROCM_RUNTIME_H */
