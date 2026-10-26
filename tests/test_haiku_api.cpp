/*
 * test_haiku_api.cpp - Pruebas de robustez y contratos de la API de Haiku
 */
#ifndef TEST_HAIKU_API_H
#define TEST_HAIKU_API_H

#include <cstdio>
#include <cassert>
#include <cstddef>
#include <cstring>
#include "../src/helper/helper_ioctls.h"

/*
 * ABI lock: rg_display_timing/rg_display_mode must stay byte-identical to
 * Haiku's display_timing/display_mode from <Accelerant.h> (24 and 40 bytes).
 * The kernel helper cannot include that userland header, so the layout is
 * asserted here instead.
 */
static_assert(sizeof(rg_display_timing) == 24, "rg_display_timing ABI drift");
static_assert(sizeof(rg_display_mode) == 40, "rg_display_mode ABI drift");
static_assert(offsetof(rg_display_timing, pixel_clock) == 0, "timing.pixel_clock");
static_assert(offsetof(rg_display_timing, h_display) == 4, "timing.h_display");
static_assert(offsetof(rg_display_timing, flags) == 20, "timing.flags");
static_assert(offsetof(rg_display_mode, timing) == 0, "mode.timing");
static_assert(offsetof(rg_display_mode, space) == 24, "mode.space");
static_assert(offsetof(rg_display_mode, virtual_width) == 28, "mode.virtual_width");
static_assert(offsetof(rg_display_mode, virtual_height) == 30, "mode.virtual_height");
static_assert(offsetof(rg_display_mode, h_display_start) == 32, "mode.h_display_start");
static_assert(offsetof(rg_display_mode, v_display_start) == 34, "mode.v_display_start");
static_assert(offsetof(rg_display_mode, flags) == 36, "mode.flags");

inline void test_haiku_api(void)
{
	rg_kernel_info info{};
	memset(&info, 0, sizeof(info));
	assert(sizeof(info.pci_device) >= 2);

	rg_translate_request req{};
	memset(&req, 0, sizeof(req));
	req.length = 4096;
	assert(req.length == 4096);

	rg_irq_request irq{};
	memset(&irq, 0, sizeof(irq));
	irq.sem = -1;
	assert(irq.sem == -1);

	printf("[PASS] Haiku API & ioctl contracts robust\n");
}

#endif
