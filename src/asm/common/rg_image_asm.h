/*
 * rg_image_asm.h - Interfaz común para rutinas de procesamiento de imagen optimizadas en ASM
 */
#ifndef RG_IMAGE_ASM_H
#define RG_IMAGE_ASM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Conversión optimizada de YUV420 a RGBA por bloques */
void rg_yuv420_to_rgba_simd(const uint8_t *y_plane, const uint8_t *u_plane, const uint8_t *v_plane,
                            uint8_t *rgba_out, int width, int height, int stride_y, int stride_uv);

/* Des-swizzling optimizado de texturas en mosaico */
void rg_texture_unswizzle_simd(const uint8_t *src_tiled, uint8_t *dst_linear, int width, int height, int bpp);

#ifdef __cplusplus
}
#endif

#endif /* RG_IMAGE_ASM_H */
