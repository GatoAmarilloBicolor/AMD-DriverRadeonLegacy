/*
 * radeon_simd_dispatch.cpp - Implementación del init-time dispatch SIMD
 */
#include "radeon_simd_dispatch.h"

#if defined(__x86_64__) || defined(_M_X64)
#include <cpuid.h>
#endif

static bool g_cached_avx2 = false;
static bool g_cached_neon = false;
static bool g_simd_initialized = false;

void radeon_init_simd_dispatch(void)
{
	if (g_simd_initialized)
		return;

#if defined(__x86_64__) || defined(_M_X64)
	unsigned int eax, ebx, ecx, edx;
	if (__get_cpuid(7, &eax, &ebx, &ecx, &edx)) {
		g_cached_avx2 = (ebx & (1 << 5)) != 0;
	}
#elif defined(__aarch64__) || defined(_M_ARM64)
	g_cached_neon = true; /* ARM64 asume NEON estándar */
#endif

	g_simd_initialized = true;
}

bool radeon_has_avx2(void)
{
	if (!g_simd_initialized)
		radeon_init_simd_dispatch();
	return g_cached_avx2;
}

bool radeon_has_neon(void)
{
	if (!g_simd_initialized)
		radeon_init_simd_dispatch();
	return g_cached_neon;
}
