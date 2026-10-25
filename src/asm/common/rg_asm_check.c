/*
 * rg_asm_check.c - Suite diferencial K1..K13 (A1 / §8A.3).
 *
 * Toda funcion acelerada se compara contra su referencia en C sobre datos
 * aleatorios (Mersenne Twister) antes de permitir su uso.
 */
#include "rg_asm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

/* ------------------------- MT19937 ------------------------- */
typedef struct { uint32_t mt[624]; uint32_t idx; } mt19937;

static void mt_init(mt19937 *m, uint32_t seed)
{
	m->mt[0] = seed;
	for (uint32_t i = 1; i < 624; i++)
		m->mt[i] = 1812433253u * (m->mt[i - 1] ^ (m->mt[i - 1] >> 30)) + i;
	m->idx = 624;
}

static uint32_t mt_next(mt19937 *m)
{
	if (m->idx >= 624) {
		for (uint32_t i = 0; i < 624; i++) {
			uint32_t y = (m->mt[i] & 0x80000000u) |
				(m->mt[(i + 1) % 624] & 0x7FFFFFFFu);
			m->mt[i] = m->mt[(i + 397) % 624] ^ (y >> 1) ^
				((y & 1) ? 0x9908B0DFu : 0);
		}
		m->idx = 0;
	}
	uint32_t y = m->mt[m->idx++];
	y ^= y >> 11;
	y ^= (y << 7) & 0x9D2C5680u;
	y ^= (y << 15) & 0xEFC60000u;
	y ^= y >> 18;
	return y;
}

static int g_fail = 0;
static int g_pass = 0;

static void report(const char *name, int ok, const char *detail)
{
	if (ok) {
		g_pass++;
		printf("  [PASS] %-24s %s\n", name, detail ? detail : "");
	} else {
		g_fail++;
		printf("  [FAIL] %-24s %s\n", name, detail ? detail : "");
	}
}

/* ------------------------- K1 ------------------------- */
static void test_k1(mt19937 *rng)
{
	enum { N = 4096 };
	uint32_t src[N], a[N], b[N];
	for (int i = 0; i < N; i++)
		src[i] = mt_next(rng);
	rg_k1_argb_to_rgba_ref(a, src, N);
	rg_k1_argb_to_rgba(b, src, N);
	/* comprobar contra la definicion: rotacion de bytes */
	int ok = 1;
	for (int i = 0; i < N; i++) {
		uint32_t expect = (src[i] << 8) | (src[i] >> 24);
		if (a[i] != expect || b[i] != a[i]) { ok = 0; break; }
	}
	report("K1 argb->rgba", ok, "4096 px, ref vs despacho");

	/* colas no multiplas del bloque SIMD */
	ok = 1;
	for (int n = 0; n < 70; n++) {
		for (int i = 0; i < n; i++) src[i] = mt_next(rng);
		rg_k1_argb_to_rgba_ref(a, src, n);
		rg_k1_argb_to_rgba(b, src, n);
		if (memcmp(a, b, (size_t)n * 4) != 0) { ok = 0; break; }
	}
	report("K1 tails", ok, "n=0..69");
}

/* ------------------------- K2/K3 ------------------------- */
static void encode_blocks(uint8_t *tiled, const uint8_t *linear, uint32_t w,
	uint32_t h, uint32_t block, uint32_t bpp)
{
	uint32_t tiles_x = (w + block - 1) / block;
	uint32_t tiles_y = (h + block - 1) / block;
	size_t off = 0;
	for (uint32_t ty = 0; ty < tiles_y; ty++) {
		for (uint32_t tx = 0; tx < tiles_x; tx++) {
			for (uint32_t y = 0; y < block; y++) {
				for (uint32_t x = 0; x < block; x++) {
					uint32_t px = tx * block + x;
					uint32_t py = ty * block + y;
					if (px < w && py < h)
						memcpy(tiled + off, linear + ((size_t)py * w + px) * bpp, bpp);
					else
						memset(tiled + off, 0, bpp);
					off += bpp;
				}
			}
		}
	}
}

static void test_detile(mt19937 *rng, const char *name, int addr2)
{
	const uint32_t w = 67, h = 53, bpp = 4;
	uint32_t block = addr2 ? 4 : 8;
	size_t linear_bytes = (size_t)w * h * bpp;
	uint32_t tiles_x = (w + block - 1) / block;
	uint32_t tiles_y = (h + block - 1) / block;
	size_t tiled_bytes = (size_t)tiles_x * block * tiles_y * block * bpp;

	uint8_t *linear = malloc(linear_bytes);
	uint8_t *tiled = malloc(tiled_bytes);
	uint8_t *out = malloc(linear_bytes);
	if (!linear || !tiled || !out) { report(name, 0, "alloc"); goto out; }
	for (size_t i = 0; i < linear_bytes; i++)
		linear[i] = (uint8_t)mt_next(rng);
	encode_blocks(tiled, linear, w, h, block, bpp);
	if (addr2)
		rg_k3_detile_addr2_ref(out, tiled, w, h, block, bpp);
	else
		rg_k2_detile_terascale_ref(out, tiled, w, h, block, bpp);
	int ok = memcmp(out, linear, linear_bytes) == 0;
	report(name, ok, "67x53 roundtrip");
out:
	free(linear); free(tiled); free(out);
}

/* ------------------------- K4 ------------------------- */
static void test_k4(void)
{
	float in[5] = { -1.0f, 0.0f, 0.5f, 1.0f, 2.0f };
	uint16_t out[5];
	rg_k4_pack_f32_unorm16(out, in, 5);
	int ok = out[0] == 0 && out[1] == 0 && out[2] == 32768 &&
		out[3] == 65535 && out[4] == 65535;
	report("K4 f32->unorm16", ok, "clamp y redondeo");
}

/* ------------------------- K5/K6 ------------------------- */
static void test_k6(mt19937 *rng)
{
	enum { N = 10000 };
	uint8_t *src = malloc(N), *a = malloc(N), *b = malloc(N);
	for (int i = 0; i < N; i++) src[i] = (uint8_t)mt_next(rng);
	memset(a, 0, N); memset(b, 0, N);
	rg_k6_copy_nt_ref(a, src, N);
	rg_k6_copy_nt(b, src, N);
	int ok = memcmp(a, src, N) == 0 && memcmp(b, a, N) == 0;
	report("K6 copy nt", ok, "10000 B ref vs despacho");
	free(src); free(a); free(b);
}

/* ------------------------- K7 ------------------------- */
static void test_k7(void)
{
	uint8_t y[4] = { 128, 128, 128, 128 };
	uint8_t uv[2] = { 128, 128 };
	float out[16];
	rg_k7_nv12_to_rgba_f32(out, y, uv, 2, 2, 2, 2);
	int ok = 1;
	for (int i = 0; i < 4; i++) {
		float expect = 128.0f / 255.0f;
		if (fabsf(out[i * 4 + 0] - expect) > 1e-5f) ok = 0;
		if (fabsf(out[i * 4 + 3] - 1.0f) > 1e-6f) ok = 0;
	}
	report("K7 NV12->RGBA f32", ok, "gris neutro");
}

/* ------------------------- K8 ------------------------- */
static void test_k8(void)
{
	uint8_t pat[16], buf[4096];
	for (int i = 0; i < 16; i++) pat[i] = (uint8_t)(i * 17);
	memset(buf, 0, sizeof(buf));
	rg_k8_memset128(buf, pat, sizeof(buf));
	int ok = 1;
	for (size_t i = 0; i < sizeof(buf); i++)
		if (buf[i] != pat[i & 15]) { ok = 0; break; }
	/* Cola no multiple de 32 B (AVX2) ni de 16 B (NEON). */
	uint8_t tail[4095];
	memset(tail, 0, sizeof(tail));
	rg_k8_memset128(tail, pat, sizeof(tail));
	for (size_t i = 0; i < sizeof(tail) && ok; i++)
		if (tail[i] != pat[i & 15]) ok = 0;
	report("K8 memset128", ok, "4096 B + cola 4095 B");
}

/* ------------------------- K9/K10 ------------------------- */
static void test_f16(void)
{
	float vals[6] = { 0.0f, 1.0f, -2.0f, 0.5f, 65504.0f, -0.25f };
	uint16_t h[6];
	float back[6];
	rg_k10_f32_to_f16(h, vals, 6);
	rg_k9_f16_to_f32(back, h, 6);
	int ok = 1;
	for (int i = 0; i < 6; i++) {
		if (fabsf(back[i] - vals[i]) > 0.001f * (fabsf(vals[i]) + 1.0f))
			ok = 0;
	}
	report("K9/K10 f16 roundtrip", ok, "6 valores");
}

/* ------------------------- K11 ------------------------- */
static void test_k11(void)
{
	uint32_t packed[3] = { 0xAA112233u, 0x7F445566u, 0x00123456u };
	uint32_t depth[3];
	uint8_t stencil[3];
	rg_k11_split_d24s8(depth, stencil, packed, 3);
	int ok = depth[0] == 0x112233 && stencil[0] == 0xAA &&
		depth[1] == 0x445566 && stencil[1] == 0x7F &&
		depth[2] == 0x123456 && stencil[2] == 0x00;
	report("K11 split D24S8", ok, "3 px");
}

/* ------------------------- K12 ------------------------- */
static void test_k12(void)
{
	uint32_t dst[4] = { 1, 2, 3, 4 };
	uint32_t src[4] = { 10, 20, 30, 40 };
	rg_k12_atomic_sum(dst, src, 4);
	int ok = dst[0] == 11 && dst[1] == 22 && dst[2] == 33 && dst[3] == 44;
	report("K12 atomic sum", ok, "4 lanes");
}

/* ------------------------- K13 ------------------------- */
static void test_k13(mt19937 *rng)
{
	enum { N = 1024 };
	uint32_t src[N], mid[N], back[N];
	for (int i = 0; i < N; i++) src[i] = mt_next(rng);
	rg_k13_swap_endian(mid, src, N);
	rg_k13_swap_endian(back, mid, N);
	int ok = memcmp(src, back, sizeof(src)) == 0;
	for (int i = 0; i < N && ok; i++) {
		uint32_t v = src[i];
		uint32_t want = (v >> 24) | ((v >> 8) & 0xFF00u) |
			((v << 8) & 0xFF0000u) | (v << 24);
		if (mid[i] != want) ok = 0;
	}
	/* Cola no multiple de 8 dwords (AVX2) ni de 4 (NEON). */
	enum { M = 1021 };
	uint32_t odd[M], odd_out[M];
	for (int i = 0; i < M; i++) odd[i] = src[i];
	rg_k13_swap_endian(odd_out, odd, M);
	for (int i = 0; i < M && ok; i++) {
		uint32_t v = odd[i];
		uint32_t want = (v >> 24) | ((v >> 8) & 0xFF00u) |
			((v << 8) & 0xFF0000u) | (v << 24);
		if (odd_out[i] != want) ok = 0;
	}
	report("K13 swap endian", ok, "1024 + cola 1021 dwords");
}

int main(void)
{
	mt19937 rng;
	mt_init(&rng, 0xC0FFEEu);
	printf("rg_asm_check: suite diferencial K1-K13\n");
	test_k1(&rng);
	test_detile(&rng, "K2 detile TeraScale", 0);
	test_detile(&rng, "K3 detile ADDR2", 1);
	test_k4();
	test_k6(&rng);
	test_k7();
	test_k8();
	test_f16();
	test_k11();
	test_k12();
	test_k13(&rng);
	printf("rg_asm_check: %d passed, %d failed\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
