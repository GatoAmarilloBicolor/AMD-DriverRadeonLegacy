/*
 * rg_asm_reference_impl.cpp - Referencias en C y pruebas diferenciales para kernels K1-K13 (Fase A1)
 */
#include "rg_asm.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <random>

extern "C" {

/* K1: Conversión ARGB8888 a RGBA8888 (Referencia C) */
void rg_k1_argb_to_rgba_ref(uint8_t *dst, const uint8_t *src, size_t pixel_count)
{
	if (!dst || !src) return;
	for (size_t i = 0; i < pixel_count; ++i) {
		size_t idx = i * 4;
		uint8_t a = src[idx + 0];
		uint8_t r = src[idx + 1];
		uint8_t g = src[idx + 2];
		uint8_t b = src[idx + 3];

		dst[idx + 0] = r;
		dst[idx + 1] = g;
		dst[idx + 2] = b;
		dst[idx + 3] = a;
	}
}

/* K6: Copia de Memoria no Temporal / Stream Store (Referencia C) */
void rg_k6_copy_nt_ref(uint8_t *dst, const uint8_t *src, size_t size_bytes)
{
	if (!dst || !src || size_bytes == 0) return;
	std::memcpy(dst, src, size_bytes);
}

/* K8: Relleno de Buffers con Patrones / Memset 128-bit (Referencia C) */
void rg_k8_memset128_ref(uint8_t *dst, size_t size_bytes, uint32_t pattern)
{
	if (!dst || size_bytes == 0) return;
	uint32_t *p32 = reinterpret_cast<uint32_t *>(dst);
	size_t count32 = size_bytes / 4;
	for (size_t i = 0; i < count32; ++i) {
		p32[i] = pattern;
	}
}

} // extern "C"
