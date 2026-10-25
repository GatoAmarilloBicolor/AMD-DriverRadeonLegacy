/*
 * haiku_gpu_stack.h - Afinación de integración de la pila gráfica/compute en Haiku OS
 * Soporte para: MesaGL (Gallium), RADV (Vulkan 1.4), OpenCL, ZLUDA (CUDA on Vulkan) y Zink (OpenGL on Vulkan).
 */
#ifndef HAIKU_GPU_STACK_H
#define HAIKU_GPU_STACK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tipos de API cliente admitidas en la pila unificada */
enum haiku_client_api {
	HAIKU_API_MESA_GL = 0,     /* Gallium r300g/r600g/radeonsi */
	HAIKU_API_RADV_VULKAN,     /* RADV Vulkan 1.4 nativo */
	HAIKU_API_OPENCL,          /* Cómputo OpenCL / Clover */
	HAIKU_API_ZLUDA,           /* ZLUDA (CUDA API translator) */
	HAIKU_API_ZINK             /* Zink OpenGL sobre Vulkan */
};

/* Configuración de contexto de cliente en el servidor radeon_gpud */
struct haiku_stack_context {
	enum haiku_client_api api;
	uint32_t client_team_id;
	uint64_t virtual_address_space_size;
	bool use_gpuvm;            /* true para GCN+ (amdgpu), false para legacy (verify) */
	bool enable_timeline_sem;  /* Requerido para Vulkan 1.4 y ZLUDA */
};

/* Inicialización del puente de pila gráfica para la API solicitada */
int32_t haiku_gpu_stack_init(struct haiku_stack_context *ctx);

/* Envío de comandos unificado optimizado para ZLUDA / OpenCL / RADV */
int32_t haiku_gpu_stack_submit_command(uint32_t client_id, const void *cmd_stream, size_t size, uint64_t *out_fence);

#ifdef __cplusplus
}
#endif

#endif /* HAIKU_GPU_STACK_H */
