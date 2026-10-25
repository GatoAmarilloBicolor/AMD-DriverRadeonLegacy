/*
 * rg_asm.h - Kernels K1..K13 (C7 / §8A).
 *
 * Regla ADR-007: cada kernel en ensamblador tiene (1) referencia en C,
 * (2) despacho en tiempo de ejecucion y (3) prueba diferencial.
 * Los simbolos rg_k*_ref son la referencia portable; rg_k* son el despacho.
 */
#ifndef RG_ASM_H
#define RG_ASM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* K1: ARGB8888 (0xAARRGGBB en LE) -> RGBA8888 (0xRRGGBBAA) */
void rg_k1_argb_to_rgba_ref(uint32_t *dst, const uint32_t *src, size_t n);
void rg_k1_argb_to_rgba(uint32_t *dst, const uint32_t *src, size_t n);

/* K2: detile TeraScale (bloques micro/macro) -> lineal */
void rg_k2_detile_terascale_ref(uint8_t *dst, const uint8_t *src,
	uint32_t w, uint32_t h, uint32_t block, uint32_t bpp);
void rg_k2_detile_terascale(uint8_t *dst, const uint8_t *src,
	uint32_t w, uint32_t h, uint32_t block, uint32_t bpp);

/* K3: detile ADDR2 (GCN/RDNA) -> lineal. Formula provisional [VERIFICAR]. */
void rg_k3_detile_addr2_ref(uint8_t *dst, const uint8_t *src,
	uint32_t w, uint32_t h, uint32_t block, uint32_t bpp);
void rg_k3_detile_addr2(uint8_t *dst, const uint8_t *src,
	uint32_t w, uint32_t h, uint32_t block, uint32_t bpp);

/* K4: float32 -> unorm16 */
void rg_k4_pack_f32_unorm16(uint16_t *dst, const float *src, size_t n);

/* K5: mantenimiento de cache (flush) para plataformas no coherentes */
void rg_k5_flush_cache(const void *ptr, size_t bytes);
void rg_k5_invalidate_cache(const void *ptr, size_t bytes);

/* K6: copia no temporal (stream store) */
void rg_k6_copy_nt_ref(void *dst, const void *src, size_t bytes);
void rg_k6_copy_nt(void *dst, const void *src, size_t bytes);

/* K7: NV12 -> RGBA float32 */
void rg_k7_nv12_to_rgba_f32(float *dst, const uint8_t *y, const uint8_t *uv,
	uint32_t w, uint32_t h, uint32_t y_stride, uint32_t uv_stride);

/* K8: memset con patron de 128 bits */
void rg_k8_memset128(void *dst, const uint8_t pattern[16], size_t bytes);

/* K9/K10: float16 <-> float32 */
void rg_k9_f16_to_f32(float *dst, const uint16_t *src, size_t n);
void rg_k10_f32_to_f16(uint16_t *dst, const float *src, size_t n);

/* K11: separar profundidad/stencil empaquetados (D24S8) */
void rg_k11_split_d24s8(uint32_t *depth, uint8_t *stencil,
	const uint32_t *packed, size_t n);

/* K12: reduccion atomica en memoria de host */
void rg_k12_atomic_sum(uint32_t *dst, const uint32_t *src, size_t n);

/* K13: swap de endianness para dwords (IBs) */
void rg_k13_swap_endian(uint32_t *dst, const uint32_t *src, size_t n);

/* Simbolos de ensamblador disponibles (declarados solo si existen). */
#if defined(__aarch64__)
void rg_asm_swizzle_neon(uint32_t *dst, const uint32_t *src, size_t blocks16);
void rg_asm_copy_nt_neon(void *dst, const void *src, size_t blocks64);
void rg_asm_memset128_neon(void *dst, const uint8_t pattern[16], size_t blocks16);
void rg_asm_swap_endian_neon(uint32_t *dst, const uint32_t *src, size_t blocks4);
#elif defined(__x86_64__)
void rg_asm_swizzle_avx2(uint32_t *dst, const uint32_t *src, size_t blocks32);
void rg_asm_copy_nt_avx2(void *dst, const void *src, size_t blocks32);
void rg_asm_memset128_avx2(void *dst, const uint8_t pattern[16], size_t blocks32);
void rg_asm_swap_endian_avx2(uint32_t *dst, const uint32_t *src, size_t blocks8);
#endif

#ifdef __cplusplus
}
#endif

#endif /* RG_ASM_H */
