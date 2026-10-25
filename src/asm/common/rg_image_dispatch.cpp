/*
 * rg_image_dispatch.cpp - Despacho unificado con comprobación de CPUID para AVX2/NEON
 */
#include "rg_image_asm.h"
#include <cstring>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#include <cpuid.h>
#endif

extern "C" {
#if defined(__x86_64__) || defined(_M_X64)
void _rg_yuv420_to_rgba_avx2(const uint8_t *y, const uint8_t *u, const uint8_t *v, uint8_t *out, int w, int h);
#elif defined(__aarch64__) || defined(_M_ARM64)
void __rg_yuv420_to_rgba_neon(const uint8_t *y, const uint8_t *u, const uint8_t *v, uint8_t *out, int w, int h);
#endif
}

static bool cpu_has_avx2()
{
#if defined(__x86_64__) || defined(_M_X64)
	unsigned int eax, ebx, ecx, edx;
	if (__get_cpuid(7, &eax, &ebx, &ecx, &edx)) {
		return (ebx & (1 << 5)) != 0; // Bit 5 de EBX indica soporte AVX2
	}
#endif
	return false;
}

void rg_yuv420_to_rgba_simd(const uint8_t *y_plane, const uint8_t *u_plane, const uint8_t *v_plane,
                            uint8_t *rgba_out, int width, int height, int stride_y, int stride_uv)
{
	if (!y_plane || !u_plane || !v_plane || !rgba_out)
		return;
	
#if defined(__x86_64__) || defined(_M_X64)
	if (cpu_has_avx2()) {
		_rg_yuv420_to_rgba_avx2(y_plane, u_plane, v_plane, rgba_out, width, height);
		return;
	}
#elif defined(__aarch64__) || defined(_M_ARM64)
	__rg_yuv420_to_rgba_neon(y_plane, u_plane, v_plane, rgba_out, width, height);
	return;
#endif

	/* Fallback escalar seguro si no hay soporte SIMD verificado */
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			int idx = (y * width + x) * 4;
			rgba_out[idx + 0] = y_plane[y * stride_y + x];
			rgba_out[idx + 1] = y_plane[y * stride_y + x];
			rgba_out[idx + 2] = y_plane[y * stride_y + x];
			rgba_out[idx + 3] = 255;
		}
	}
}

void rg_texture_unswizzle_simd(const uint8_t *src_tiled, uint8_t *dst_linear, int width, int height, int bpp)
{
	if (!src_tiled || !dst_linear)
		return;
	size_t total_size = (size_t)width * height * (bpp / 8);
	std::memcpy(dst_linear, src_tiled, total_size);
}
