/*
 * r600_streamout_compute.cpp - Implementación de Transform Feedback para Cómputo en R600
 * Emula Compute Shaders en R600/R700 (VLIW5) mediante Vertex Shader + Stream-Out (Transform Feedback).
 * Referencia: §8.14 PLAN_RADEON_LEGACY_HAIKU-2.md
 */
#include "r600_streamout_compute.h"

#include <cstring>
#include <cstdint>

/* ------------------------------------------------------------------ */
/* Constantes PM4 para R600 (VLIW5)                                   */
/* ------------------------------------------------------------------ */

enum r600_pm4_type3_opcode {
    R600_PM4_SET_CONFIG_REG    = 0x14,
    R600_PM4_SET_CONTEXT_REG   = 0x15,
    R600_PM4_SET_SH_REG        = 0x16,
    R600_PM4_DRAW_INDEX_AUTO   = 0x33,
    R600_PM4_DRAW_INDEX_OFFSET = 0x34,
    R600_PM4_EVENT_WRITE       = 0x20,
};

enum r600_config_reg {
    R600_SQ_PGM_START_VS       = 0x2883C, /* Inicio programa Vertex Shader */
    R600_SQ_PGM_RESOURCES_VS   = 0x28840, /* Recursos VS (GPRs, stack) */
    R600_SQ_PGM_CF_OFFSET_VS   = 0x28844, /* Offset control flow VS */
    R600_VGT_STRMOUT_CONFIG    = 0x28A60, /* Config Stream-Out */
    R600_VGT_STRMOUT_BUFFER_CONFIG = 0x28A64, /* Buffer Stream-Out */
    R600_VGT_STRMOUT_BUFFER_BASE  = 0x28A68, /* Base buffer Stream-Out */
    R600_VGT_STRMOUT_BUFFER_SIZE  = 0x28A6C, /* Tamaño buffer Stream-Out */
    R600_VGT_VERTEX_REUSE_BLOCK_CNTL = 0x28C5C,
    R600_PA_SC_AA_CONFIG       = 0x28C04,
    R600_SPI_SHADER_USER_DATA_VS_0 = 0x286E0, /* User data VS (argumentos kernel) */
};

enum r600_context_reg {
    R600_VGT_PRIMITIVE_TYPE    = 0x28A40, /* Tipo primitiva (POINTLIST para compute) */
    R600_VGT_NUM_INSTANCES     = 0x28A44,
    R600_VGT_INDEX_TYPE        = 0x28A70,
    R600_VGT_NUM_VERTICES      = 0x28A74,
    R600_PA_CL_VPORT_XSCALE    = 0x28B00,
    R600_PA_CL_VPORT_XOFFSET   = 0x28B04,
    R600_PA_CL_VPORT_YSCALE    = 0x28B08,
    R600_PA_CL_VPORT_YOFFSET   = 0x28B0C,
    R600_PA_CL_VPORT_ZSCALE    = 0x28B10,
    R600_PA_CL_VPORT_ZOFFSET   = 0x28B14,
    R600_PA_SC_WINDOW_OFFSET   = 0x28B20,
    R600_PA_SC_CLIPRECT_RULE   = 0x28B24,
    R600_PA_SC_EDGERULE        = 0x28B28,
};



/* ------------------------------------------------------------------ */
/* Helpers PM4                                                         */
/* ------------------------------------------------------------------ */

static inline uint32_t pm4_type3(uint32_t opcode, uint32_t count)
{
    return (3u << 30) | ((opcode & 0xFF) << 8) | ((count - 1) & 0x3FFF);
}

static inline uint32_t pm4_set_config_reg(uint32_t reg, uint32_t count)
{
    return (3u << 30) | (R600_PM4_SET_CONFIG_REG << 8) | (((count * 2) - 1) & 0x3FFF);
}

static inline uint32_t pm4_set_context_reg(uint32_t reg, uint32_t count)
{
    return (3u << 30) | (R600_PM4_SET_CONTEXT_REG << 8) | (((count * 2) - 1) & 0x3FFF);
}

static inline uint32_t pm4_set_sh_reg(uint32_t reg, uint32_t count)
{
    return (3u << 30) | (R600_PM4_SET_SH_REG << 8) | (((count * 2) - 1) & 0x3FFF);
}

/* ------------------------------------------------------------------ */
/* Compilación de pipeline compute -> PM4 streamout                    */
/* ------------------------------------------------------------------ */

bool r600_compile_compute_to_streamout(const struct r600_compute_pipeline *pipeline, uint32_t *out_pm4_buffer, uint32_t *out_dwords)
{
    if (pipeline == nullptr || out_pm4_buffer == nullptr || out_dwords == nullptr)
        return false;

    if (pipeline->binary_size == 0 || pipeline->binary == nullptr)
        return false; /* Requiere binario VS compilado (R600 ISA) */

    uint32_t dwords = 0;
    uint32_t *buf = out_pm4_buffer;
    uint32_t max_dwords = *out_dwords;
    
    #define EMIT(dw) do { if (dwords >= max_dwords) return false; buf[dwords++] = (dw); } while (0)
    #define EMIT_PAIR(reg, val) do { EMIT(reg); EMIT(val); } while (0)

    /* 1. Deshabilitar pipeline previo */
    EMIT(pm4_type3(R600_PM4_EVENT_WRITE, 2));
    EMIT(R600_PM4_EVENT_WRITE);
    EMIT(0x00000010); /* CS_PARTIAL_FLUSH */

    /* 2. Configurar Vertex Shader como Compute Shader */
    EMIT(pm4_set_config_reg(R600_SQ_PGM_START_VS, 1));
    EMIT_PAIR(R600_SQ_PGM_START_VS, pipeline->vs_binary_addr); /* GPU addr del binario VS */

    EMIT(pm4_set_config_reg(R600_SQ_PGM_RESOURCES_VS, 1));
    EMIT_PAIR(R600_SQ_PGM_RESOURCES_VS,
        (pipeline->num_gprs & 0x3F) |       /* GPRs */
        ((pipeline->stack_size & 0xFF) << 8) | /* Stack size */
        (1u << 16));                        /* Single thread mode off */

    if (pipeline->cf_offset) {
        EMIT(pm4_set_config_reg(R600_SQ_PGM_CF_OFFSET_VS, 1));
        EMIT_PAIR(R600_SQ_PGM_CF_OFFSET_VS, pipeline->cf_offset);
    }

    /* 3. Configurar Stream-Out (Transform Feedback) */
    EMIT(pm4_set_config_reg(R600_VGT_STRMOUT_CONFIG, 1));
    EMIT_PAIR(R600_VGT_STRMOUT_CONFIG,
        (pipeline->num_outputs & 0xF) |           /* Num outputs */
        ((pipeline->streamout_stride & 0xFFF) << 4) | /* Stride en dwords */
        (1u << 16));                              /* Enable */

    /* 4. Configurar buffers de Stream-Out (hasta 4 buffers) */
    for (uint32_t i = 0; i < pipeline->num_outputs && i < 4; i++) {
        EMIT(pm4_set_config_reg(R600_VGT_STRMOUT_BUFFER_CONFIG + i * 4, 1));
        EMIT_PAIR(R600_VGT_STRMOUT_BUFFER_CONFIG + i * 4,
            (pipeline->output_buffers[i].gpu_addr & 0xFFFFFFFF) | /* Base addr low */
            ((pipeline->output_buffers[i].size & 0xFFFFFFF) << 4) /* Size en bytes */
        );
        
        EMIT(pm4_set_config_reg(R600_VGT_STRMOUT_BUFFER_BASE + i * 4, 1));
        EMIT_PAIR(R600_VGT_STRMOUT_BUFFER_BASE + i * 4,
            (pipeline->output_buffers[i].gpu_addr >> 32) & 0xFF); /* Base addr high */
        
        EMIT(pm4_set_config_reg(R600_VGT_STRMOUT_BUFFER_SIZE + i * 4, 1));
        EMIT_PAIR(R600_VGT_STRMOUT_BUFFER_SIZE + i * 4,
            pipeline->output_buffers[i].size); /* Tamaño en bytes */
    }

    /* 5. Configurar primitivas tipo POINTLIST (1 vértice = 1 work-item) */
    EMIT(pm4_set_context_reg(R600_VGT_PRIMITIVE_TYPE, 1));
    EMIT_PAIR(R600_VGT_PRIMITIVE_TYPE, 0x0); /* POINTLIST */

    /* 6. Configurar viewport y scissor para work-items */
    EMIT(pm4_set_context_reg(R600_PA_CL_VPORT_XSCALE, 1));
    EMIT_PAIR(R600_PA_CL_VPORT_XSCALE, 0x3F800000); /* 1.0f */
    EMIT(pm4_set_context_reg(R600_PA_CL_VPORT_XOFFSET, 1));
    EMIT_PAIR(R600_PA_CL_VPORT_XOFFSET, 0x00000000);
    EMIT(pm4_set_context_reg(R600_PA_CL_VPORT_YSCALE, 1));
    EMIT_PAIR(R600_PA_CL_VPORT_YSCALE, 0x3F800000);
    EMIT(pm4_set_context_reg(R600_PA_CL_VPORT_YOFFSET, 1));
    EMIT_PAIR(R600_PA_CL_VPORT_YOFFSET, 0x00000000);

    /* 7. Configurar user data VS (argumentos del kernel compute) */
    for (uint32_t i = 0; i < pipeline->num_user_sgprs && i < 32; i++) {
        EMIT(pm4_set_sh_reg(R600_SPI_SHADER_USER_DATA_VS_0 + i, 1));
        EMIT_PAIR(R600_SPI_SHADER_USER_DATA_VS_0 + i, pipeline->user_sgprs[i]);
    }

    /* 8. Configurar instancia count = num_workgroups * workgroup_size */
    uint32_t total_workitems = pipeline->num_groups_x * pipeline->num_groups_y * pipeline->num_groups_z *
                               pipeline->workgroup_size_x * pipeline->workgroup_size_y * pipeline->workgroup_size_z;
    
    EMIT(pm4_set_context_reg(R600_VGT_NUM_INSTANCES, 1));
    EMIT_PAIR(R600_VGT_NUM_INSTANCES, 1);
    
    EMIT(pm4_set_context_reg(R600_VGT_NUM_VERTICES, 1));
    EMIT_PAIR(R600_VGT_NUM_VERTICES, total_workitems);

    /* 9. DRAW_INDEX_AUTO para lanzar work-items */
    EMIT(pm4_type3(R600_PM4_DRAW_INDEX_AUTO, 4));
    EMIT(0x00000000); /* Indice base */
    EMIT(total_workitems); /* Num vertices */
    EMIT(1); /* Num instancias */
    EMIT(0); /* Padding */

    /* 10. Flush cache y event write */
    EMIT(pm4_type3(R600_PM4_EVENT_WRITE, 2));
    EMIT(R600_PM4_EVENT_WRITE);
    EMIT(0x0000000A); /* CACHE_FLUSH_AND_INV */

    *out_dwords = dwords;
    return true;
}

/* ------------------------------------------------------------------ */
/* Ejecución de pipeline streamout                                     */
/* ------------------------------------------------------------------ */

bool r600_execute_streamout(const struct r600_streamout_context *ctx,
                            const struct r600_streamout_dispatch *dispatch)
{
    if (ctx == nullptr || dispatch == nullptr)
        return false;

    /* En servidor real: submit PM4 buffer a RingManager */
    /* Aquí solo validamos parámetros */
    
    if (ctx->pm4_buffer == nullptr || ctx->pm4_dwords == 0)
        return false;
    
    if (dispatch->num_groups_x == 0 || dispatch->num_groups_y == 0 || dispatch->num_groups_z == 0)
        return false;

    /* Validar buffers de salida */
    for (uint32_t i = 0; i < ctx->pipeline.num_outputs; i++) {
        if (ctx->pipeline.output_buffers[i].gpu_addr == 0 ||
            ctx->pipeline.output_buffers[i].size == 0)
            return false;
    }

    return true;
}

/* ------------------------------------------------------------------ */
/* Limpieza / Reset                                                    */
/* ------------------------------------------------------------------ */

void r600_streamout_reset(void)
{
    /* En servidor real: soft reset del motor 3D + Stream-Out */
}