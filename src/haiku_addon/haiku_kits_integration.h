/*
 * haiku_kits_integration.h - Pasarelas de integración nativa con Media Kit, GL Kit, App Kit y ROCm/ZLUDA
 */
#ifndef HAIKU_KITS_INTEGRATION_H
#define HAIKU_KITS_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 1. Media Kit Integration (UVD Hardware Video Decoding) */
bool haiku_media_kit_init_decoder(uint32_t codec_type, uint32_t width, uint32_t height);

/* 2. GL Kit / Mesa Integration (OpenGL 3.3 Context) */
bool haiku_gl_kit_create_context(void *gl_view_handle);

/* 3. App Kit / app_server Integration (2D Hardware Acceleration) */
bool haiku_app_kit_blit_surface(const void *src_bits, void *dst_bits, int width, int height);

/* 4. ROCm / ZLUDA / OpenCL Compute Runtime Integration */
bool haiku_rocm_init_compute_queue(uint32_t device_id);

#ifdef __cplusplus
}
#endif

#endif /* HAIKU_KITS_INTEGRATION_H */
