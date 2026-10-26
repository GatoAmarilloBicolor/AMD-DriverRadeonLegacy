/*
 * fsr_scaler.h - AMD FidelityFX Super Resolution 2.x/3.x open-source
 * Implementación completa: EASU (spatial upscaling) + RCAS (sharpening) + temporal accumulation
 * Compatible con FSR 2.x/3.x API. Optimizado para GPUs legacy con SIMD (AVX2/NEON).
 */
#ifndef FSR_SCALER_H
#define FSR_SCALER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Quality modes (match FSR 2.x/3.x) */
typedef enum fsr_quality_mode {
    FSR_QUALITY_MODE_NATIVE_AA = 0,  /* Native AA (no upscale) */
    FSR_QUALITY_MODE_QUALITY   = 1,  /* 1.5x scale */
    FSR_QUALITY_MODE_BALANCED  = 2,  /* 1.7x scale */
    FSR_QUALITY_MODE_PERFORMANCE = 3, /* 2.0x scale */
    FSR_QUALITY_MODE_ULTRA_PERFORMANCE = 4 /* 3.0x scale */
} fsr_quality_mode_t;

/* Resource format */
typedef enum fsr_format {
    FSR_FORMAT_R8G8B8A8_UNORM = 0,
    FSR_FORMAT_R16G16B16A16_FLOAT = 1,
    FSR_FORMAT_R11G11B10_FLOAT = 2,
    FSR_FORMAT_R32_FLOAT = 3
} fsr_format_t;

/* Resource description */
typedef struct fsr_resource {
    void *ptr;              /* CPU/GPU pointer */
    uint32_t width;
    uint32_t height;
    uint32_t stride;        /* bytes per row */
    fsr_format_t format;
} fsr_resource_t;

/* Constant buffer for EASU */
typedef struct fsr_easu_constants {
    float input_size[2];
    float output_size[2];
    float rcp_input_size[2];
    float rcp_output_size[2];
    float jitter_offset[2];
    float downscale_factor[2];
    float rcp_downscale_factor[2];
    float pad[2];
} fsr_easu_constants_t;

/* Constant buffer for RCAS */
typedef struct fsr_rcas_constants {
    float input_size[2];
    float rcp_input_size[2];
    float sharpness;
    float pad;
} fsr_rcas_constants_t;

/* Main context */
typedef struct fsr_context {
    /* Configuration */
    fsr_quality_mode_t quality_mode;
    uint32_t display_width;
    uint32_t display_height;
    uint32_t render_width;
    uint32_t render_height;
    
    /* Internal resources */
    fsr_resource_t color_history[2];    /* Ping-pong history buffers */
    fsr_resource_t internal_buffer;     /* Intermediate buffer */
    fsr_resource_t luma_history[2];     /* Luma for reactiveness */
    fsr_resource_t motion_vectors;      /* Motion vectors */
    fsr_resource_t depth_buffer;        /* Depth buffer */
    fsr_resource_t exposure_texture;    /* Auto-exposure */
    fsr_resource_t reactivity_mask;     /* Reactiveness mask */
    
    /* State */
    uint32_t frame_index;
    bool reset_history;
    float previous_jitter[2];
    
    /* Constants */
    fsr_easu_constants_t easu_constants;
    fsr_rcas_constants_t rcas_constants;
    
    /* Stats */
    uint64_t frame_time_us;
    uint32_t history_frames;
} fsr_context_t;

/* API */

/* Initialize FSR context */
bool fsr_init(fsr_context_t *ctx, 
              fsr_quality_mode_t quality,
              uint32_t display_width, uint32_t display_height,
              uint32_t render_width, uint32_t render_height);

/* Cleanup */
void fsr_shutdown(fsr_context_t *ctx);

/* Set quality mode at runtime */
void fsr_set_quality_mode(fsr_context_t *ctx, fsr_quality_mode_t quality);

/* Set motion vectors and depth */
void fsr_set_motion_vectors(fsr_context_t *ctx, const fsr_resource_t *mv);
void fsr_set_depth_buffer(fsr_context_t *ctx, const fsr_resource_t *depth);

/* Set jitter for temporal AA (from jitter sequence) */
void fsr_set_jitter(fsr_context_t *ctx, float jitter_x, float jitter_y);

/* Set camera parameters for depth reconstruction */
void fsr_set_camera(fsr_context_t *ctx, float tan_half_fov, float near_plane, float far_plane);

/* Set exposure for auto-exposure */
void fsr_set_exposure(fsr_context_t *ctx, const fsr_resource_t *exposure);

/* Main dispatch - renders FSR 2.x pipeline */
bool fsr_dispatch(fsr_context_t *ctx,
                  const fsr_resource_t *input_color,
                  fsr_resource_t *output_color,
                  const fsr_resource_t *motion_vectors,
                  const fsr_resource_t *depth,
                  const fsr_resource_t *exposure,
                  float jitter_x, float jitter_y);

/* Get internal stats */
void fsr_get_stats(const fsr_context_t *ctx, uint64_t *frame_time_us, uint32_t *history_frames);

/* Reaction/responsiveness control (FSR 3.x) */
void fsr_set_reactiveness(fsr_context_t *ctx, float reactiveness);

/* Frame generation (FSR 3.x - optional) */
bool fsr_enable_frame_generation(fsr_context_t *ctx, bool enable);
bool fsr_dispatch_frame_gen(fsr_context_t *ctx, fsr_resource_t *interpolated_frame);

#ifdef __cplusplus
}
#endif

#endif /* FSR_SCALER_H */