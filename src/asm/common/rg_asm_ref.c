/*
 * rg_asm_ref.c - Referencias en C y despacho de los kernels K1..K13 (§8A).
 */
#include "rg_asm.h"

#include <string.h>
#include <math.h>

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#  include <cpuid.h>
#endif

/*
 * Despacho dinamico (§8A.3): AVX2 solo si la CPU y el SO lo soportan.
 * En x86-64 sin AVX2 se usa la referencia C; en ARM64 NEON es obligatorio.
 */
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
static int cpu_has_avx2(void)
{
	static int cached = -1;
	if (cached >= 0)
		return cached;

	unsigned int eax, ebx, ecx, edx;
	if (__get_cpuid_max(0, NULL) < 7) {
		cached = 0;
		return cached;
	}
	__cpuid_count(1, 0, eax, ebx, ecx, edx);
	const unsigned int avx = 1u << 28;
	const unsigned int osxsave = 1u << 27;
	if ((ecx & avx) == 0 || (ecx & osxsave) == 0) {
		cached = 0;
		return cached;
	}

	uint32_t xcr0_lo, xcr0_hi;
	__asm__ __volatile__("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
	if ((xcr0_lo & 0x6u) != 0x6u) {
		cached = 0;
		return cached;
	}

	__cpuid_count(7, 0, eax, ebx, ecx, edx);
	cached = (ebx & (1u << 5)) != 0; /* AVX2 */
	return cached;
}
#endif

/* ---------------- K1 ---------------- */
void rg_k1_argb_to_rgba_ref(uint32_t *dst, const uint32_t *src, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		uint32_t v = src[i];
		dst[i] = (v << 8) | (v >> 24);
	}
}

void rg_k1_argb_to_rgba(uint32_t *dst, const uint32_t *src, size_t n)
{
	size_t done = 0;
#if defined(__aarch64__)
	/* El asm procesa 16 bytes (4 pixeles) por bloque. */
	size_t blocks = n / 4;
	if (blocks > 0) {
		rg_asm_swizzle_neon(dst, src, blocks);
		done = blocks * 4;
	}
#elif defined(__x86_64__)
	/* El asm procesa 32 bytes (8 pixeles) por bloque. */
	size_t blocks = n / 8;
	if (blocks > 0) {
		rg_asm_swizzle_avx2(dst, src, blocks);
		done = blocks * 8;
	}
#endif
	if (done < n)
		rg_k1_argb_to_rgba_ref(dst + done, src + done, n - done);
}

/* ---------------- K2/K3 ---------------- */
/*
 * Layout de bloques comun: el origen esta ordenado por bloques
 * (tile-row mayor, tile-col menor) y dentro de cada bloque por filas.
 * El destino es lineal (row-major). block = lado en pixeles, bpp = bytes/pixel.
 * La formula exacta de TeraScale/ADDR2 debe confirmarse con los PDFs de AMD
 * ([VERIFICAR]); aqui se implementa el layout de bloques documentado.
 */
static void detile_blocks(uint8_t *dst, const uint8_t *src, uint32_t w, uint32_t h,
	uint32_t block, uint32_t bpp)
{
	if (block == 0)
		block = 8;
	uint32_t tiles_x = (w + block - 1) / block;
	uint32_t tiles_y = (h + block - 1) / block;
	size_t src_off = 0;

	for (uint32_t ty = 0; ty < tiles_y; ty++) {
		for (uint32_t tx = 0; tx < tiles_x; tx++) {
			for (uint32_t y = 0; y < block; y++) {
				uint32_t py = ty * block + y;
				for (uint32_t x = 0; x < block; x++) {
					uint32_t px = tx * block + x;
					if (px >= w || py >= h) {
						src_off += bpp;
						continue;
					}
					const uint8_t *s = src + src_off;
					uint8_t *d = dst + ((size_t)py * w + px) * bpp;
					memcpy(d, s, bpp);
					src_off += bpp;
				}
			}
		}
	}
}

void rg_k2_detile_terascale_ref(uint8_t *dst, const uint8_t *src,
	uint32_t w, uint32_t h, uint32_t block, uint32_t bpp)
{
	detile_blocks(dst, src, w, h, block ? block : 8, bpp);
}

void rg_k2_detile_terascale(uint8_t *dst, const uint8_t *src,
	uint32_t w, uint32_t h, uint32_t block, uint32_t bpp)
{
	/* v1: sin kernel SIMD para 2D; la referencia ya es portable y el
	 * despacho queda preparado para A2/A3. */
	rg_k2_detile_terascale_ref(dst, src, w, h, block, bpp);
}

void rg_k3_detile_addr2_ref(uint8_t *dst, const uint8_t *src,
	uint32_t w, uint32_t h, uint32_t block, uint32_t bpp)
{
	detile_blocks(dst, src, w, h, block ? block : 4, bpp);
}

void rg_k3_detile_addr2(uint8_t *dst, const uint8_t *src,
	uint32_t w, uint32_t h, uint32_t block, uint32_t bpp)
{
	rg_k3_detile_addr2_ref(dst, src, w, h, block, bpp);
}

/* ---------------- K4 ---------------- */
void rg_k4_pack_f32_unorm16(uint16_t *dst, const float *src, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		float v = src[i];
		if (v < 0.0f) v = 0.0f;
		if (v > 1.0f) v = 1.0f;
		dst[i] = (uint16_t)(v * 65535.0f + 0.5f);
	}
}

/* ---------------- K5 ---------------- */
void rg_k5_flush_cache(const void *ptr, size_t bytes)
{
	(void)ptr; (void)bytes;
#if defined(__aarch64__)
	/* Flush de lineas D-cache: el rango real se alinea en A3. */
	uintptr_t p = (uintptr_t)ptr;
	uintptr_t end = p + bytes;
	for (; p < end; p += 64)
		__asm__ __volatile__("dc cvac, %0" :: "r"(p) : "memory");
	__asm__ __volatile__("dsb sy" ::: "memory");
#endif
}

void rg_k5_invalidate_cache(const void *ptr, size_t bytes)
{
	(void)ptr; (void)bytes;
#if defined(__aarch64__)
	uintptr_t p = (uintptr_t)ptr;
	uintptr_t end = p + bytes;
	for (; p < end; p += 64)
		__asm__ __volatile__("dc ivac, %0" :: "r"(p) : "memory");
	__asm__ __volatile__("dsb sy" ::: "memory");
#endif
}

/* ---------------- K6 ---------------- */
void rg_k6_copy_nt_ref(void *dst, const void *src, size_t bytes)
{
	memcpy(dst, src, bytes);
}

void rg_k6_copy_nt(void *dst, const void *src, size_t bytes)
{
	size_t done = 0;
#if defined(__aarch64__)
	size_t blocks = bytes / 64;
	if (blocks > 0) {
		rg_asm_copy_nt_neon(dst, src, blocks);
		done = blocks * 64;
	}
#elif defined(__x86_64__)
	size_t blocks = bytes / 32;
	if (blocks > 0) {
		rg_asm_copy_nt_avx2(dst, src, blocks);
		done = blocks * 32;
	}
#endif
	if (done < bytes)
		memcpy((uint8_t *)dst + done, (const uint8_t *)src + done, bytes - done);
}

/* ---------------- K7 ---------------- */
static float clamp01f(float v)
{
	if (v < 0.0f) return 0.0f;
	if (v > 1.0f) return 1.0f;
	return v;
}

void rg_k7_nv12_to_rgba_f32(float *dst, const uint8_t *y, const uint8_t *uv,
	uint32_t w, uint32_t h, uint32_t y_stride, uint32_t uv_stride)
{
	for (uint32_t py = 0; py < h; py++) {
		for (uint32_t px = 0; px < w; px++) {
			float Y = (float)y[py * y_stride + px];
			uint32_t cx = px & ~1u;
			uint32_t cy = py & ~1u;
			float U = (float)uv[(cy / 2) * uv_stride + cx] - 128.0f;
			float V = (float)uv[(cy / 2) * uv_stride + cx + 1] - 128.0f;
			float R = Y + 1.402f * V;
			float G = Y - 0.344136f * U - 0.714136f * V;
			float B = Y + 1.772f * U;
			size_t o = ((size_t)py * w + px) * 4;
			dst[o + 0] = clamp01f(R / 255.0f);
			dst[o + 1] = clamp01f(G / 255.0f);
			dst[o + 2] = clamp01f(B / 255.0f);
			dst[o + 3] = 1.0f;
		}
	}
}

/* ---------------- K8 ---------------- */
void rg_k8_memset128(void *dst, const uint8_t pattern[16], size_t bytes)
{
	uint8_t *d = (uint8_t *)dst;
	size_t done = 0;
#if defined(__aarch64__)
	size_t blocks = bytes / 16;
	if (blocks > 0) {
		rg_asm_memset128_neon(d, pattern, blocks);
		done = blocks * 16;
	}
#elif defined(__x86_64__)
	size_t blocks = cpu_has_avx2() ? bytes / 32 : 0;
	if (blocks > 0) {
		rg_asm_memset128_avx2(d, pattern, blocks);
		done = blocks * 32;
	}
#endif
	for (size_t i = done; i < bytes; i++)
		d[i] = pattern[i & 15];
}

/* ---------------- K9/K10 ---------------- */
static float f16_to_f32(uint16_t h)
{
	uint32_t sign = (h >> 15) & 1;
	uint32_t exp = (h >> 10) & 0x1F;
	uint32_t mant = h & 0x3FF;
	uint32_t f;
	if (exp == 0) {
		if (mant == 0) {
			f = sign << 31;
		} else {
			/* subnormal */
			exp = 127 - 15 + 1;
			while ((mant & 0x400) == 0) {
				mant <<= 1;
				exp--;
			}
			mant &= 0x3FF;
			f = (sign << 31) | (exp << 23) | (mant << 13);
		}
	} else if (exp == 0x1F) {
		f = (sign << 31) | 0x7F800000 | (mant << 13);
	} else {
		f = (sign << 31) | ((exp + 127 - 15) << 23) | (mant << 13);
	}
	float out;
	memcpy(&out, &f, sizeof(out));
	return out;
}

static uint16_t f32_to_f16(float value)
{
	uint32_t f;
	memcpy(&f, &value, sizeof(f));
	uint32_t sign = (f >> 16) & 0x8000;
	int32_t exp = (int32_t)((f >> 23) & 0xFF) - 127 + 15;
	uint32_t mant = f & 0x7FFFFF;
	if (((f >> 23) & 0xFF) == 0xFF)
		return (uint16_t)(sign | 0x7C00 | (mant ? 0x200 : 0));
	if (exp <= 0) {
		if (exp < -10)
			return (uint16_t)sign;
		mant |= 0x800000;
		uint32_t shift = (uint32_t)(14 - exp);
		uint32_t m = mant >> shift;
		if ((mant >> (shift - 1)) & 1)
			m++;
		return (uint16_t)(sign | m);
	}
	if (exp >= 31)
		return (uint16_t)(sign | 0x7C00);
	uint32_t half = sign | ((uint32_t)exp << 10) | (mant >> 13);
	if (mant & 0x1000)
		half++;
	return (uint16_t)half;
}

void rg_k9_f16_to_f32(float *dst, const uint16_t *src, size_t n)
{
	for (size_t i = 0; i < n; i++)
		dst[i] = f16_to_f32(src[i]);
}

void rg_k10_f32_to_f16(uint16_t *dst, const float *src, size_t n)
{
	for (size_t i = 0; i < n; i++)
		dst[i] = f32_to_f16(src[i]);
}

/* ---------------- K11 ---------------- */
void rg_k11_split_d24s8(uint32_t *depth, uint8_t *stencil,
	const uint32_t *packed, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		if (depth != NULL)
			depth[i] = packed[i] & 0x00FFFFFFu;
		if (stencil != NULL)
			stencil[i] = (uint8_t)(packed[i] >> 24);
	}
}

/* ---------------- K12 ---------------- */
void rg_k12_atomic_sum(uint32_t *dst, const uint32_t *src, size_t n)
{
	for (size_t i = 0; i < n; i++)
		dst[i] += src[i];
}

/* ---------------- K13 ---------------- */
static void swap_endian_ref(uint32_t *dst, const uint32_t *src, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		uint32_t v = src[i];
		dst[i] = (v >> 24) | ((v >> 8) & 0xFF00u) |
			((v << 8) & 0xFF0000u) | (v << 24);
	}
}

void rg_k13_swap_endian(uint32_t *dst, const uint32_t *src, size_t n)
{
	size_t done = 0;
#if defined(__aarch64__)
	size_t blocks = n / 4;
	if (blocks > 0) {
		rg_asm_swap_endian_neon(dst, src, blocks);
		done = blocks * 4;
	}
#elif defined(__x86_64__)
	size_t blocks = cpu_has_avx2() ? n / 8 : 0;
	if (blocks > 0) {
		rg_asm_swap_endian_avx2(dst, src, blocks);
		done = blocks * 8;
	}
#endif
	if (done < n)
		swap_endian_ref(dst + done, src + done, n - done);
}
