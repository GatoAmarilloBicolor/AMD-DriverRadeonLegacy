/*
 * r600_streamout_compute.h - Transform Feedback Compute Emulation para R600/R700
 * Emula Compute Shaders en GPUs VLIW5 (R600/R700/Evergreen/NI) sin hardware nativo de Compute.
 * Usa Vertex Shader + Stream-Out (Transform Feedback) para ejecutar work-items.
 * Referencia: §8.14 PLAN_RADEON_LEGACY_HAIKU-2.md, AMD R600 ISA Reference.
 */
#ifndef R600_STREAMOUT_COMPUTE_H
#define R600_STREAMOUT_COMPUTE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Buffer de salida Stream-Out */
struct r600_streamout_buffer {
    uint64_t gpu_addr;    /* Dirección GPU del buffer */
    uint32_t size;        /* Tamaño en bytes */
    uint32_t stride;      /* Stride por vértice (en dwords) */
};

/* Pipeline de compute compilado para Stream-Out */
struct r600_compute_pipeline {
    const uint8_t *binary;        /* Binario Vertex Shader (R600 ISA) */
    uint32_t binary_size;         /* Tamaño en bytes */
    uint64_t vs_binary_addr;      /* Dirección GPU donde se subió el binario */
    uint32_t num_gprs;            /* Registros GPRs usados */
    uint32_t stack_size;          /* Tamaño stack */
    uint32_t cf_offset;           /* Offset control flow */
    uint32_t num_outputs;         /* Número de outputs Stream-Out */
    uint32_t streamout_stride;    /* Stride total por vértice (dwords) */
    struct r600_streamout_buffer output_buffers[4]; /* Hasta 4 buffers */
    uint32_t num_user_sgprs;      /* User SGPRs (argumentos kernel) */
    uint32_t user_sgprs[32];      /* Valores user SGPRs */
    
    /* Workgroup configuration */
    uint32_t workgroup_size_x;
    uint32_t workgroup_size_y;
    uint32_t workgroup_size_z;
    uint32_t num_groups_x;
    uint32_t num_groups_y;
    uint32_t num_groups_z;
};

/* Contexto de ejecución Stream-Out */
struct r600_streamout_context {
    struct r600_compute_pipeline pipeline;
    uint32_t *pm4_buffer;         /* Buffer PM4 generado */
    uint32_t pm4_dwords;          /* Dwords en buffer PM4 */
};

/* Parámetros de dispatch */
struct r600_streamout_dispatch {
    uint32_t num_groups_x;
    uint32_t num_groups_y;
    uint32_t num_groups_z;
};

/* Compila pipeline compute a secuencia PM4 para Stream-Out */
bool r600_compile_compute_to_streamout(const struct r600_compute_pipeline *pipeline,
                                       uint32_t *out_pm4_buffer, uint32_t *out_dwords);

/* Ejecuta dispatch Stream-Out */
bool r600_execute_streamout(const struct r600_streamout_context *ctx,
                            const struct r600_streamout_dispatch *dispatch);

/* Reset del motor Stream-Out */
void r600_streamout_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* R600_STREAMOUT_COMPUTE_H */