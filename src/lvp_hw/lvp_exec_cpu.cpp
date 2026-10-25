/*
 * lvp_exec_cpu.cpp - Ejecutor CPU de referencia.
 */
#include "lvp_exec_cpu.h"
#include "../asm/common/rg_asm.h"

#include <cstring>

void rg_cpu_clear_rgba(uint8_t *dst, size_t pixels, uint32_t rgba)
{
	if (dst == nullptr)
		return;
	uint8_t pixel[4] = {
		(uint8_t)(rgba >> 0), (uint8_t)(rgba >> 8),
		(uint8_t)(rgba >> 16), (uint8_t)(rgba >> 24)
	};
	uint8_t pattern[16];
	for (int i = 0; i < 4; i++)
		memcpy(pattern + i * 4, pixel, 4);

	size_t groups = pixels / 4;
	if (groups > 0)
		rg_k8_memset128(dst, pattern, groups * 16);

	size_t done = groups * 4;
	for (size_t i = done; i < pixels; i++)
		memcpy(dst + i * 4, pixel, 4);
}

void rg_cpu_copy(uint8_t *dst, const uint8_t *src, size_t bytes)
{
	if (dst == nullptr || src == nullptr)
		return;
	rg_k6_copy_nt(dst, src, bytes);
}

size_t rg_cpu_compare(const uint8_t *a, const uint8_t *b, size_t bytes)
{
	if (a == nullptr || b == nullptr)
		return 0;
	for (size_t i = 0; i < bytes; i++) {
		if (a[i] != b[i])
			return i + 1;
	}
	return 0;
}
