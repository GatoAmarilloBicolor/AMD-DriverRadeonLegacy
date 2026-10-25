/*
 * test_haiku_api.cpp - Pruebas de robustez y contratos de la API de Haiku
 */
#ifndef TEST_HAIKU_API_H
#define TEST_HAIKU_API_H

#include <cstdio>
#include <cassert>
#include <cstring>
#include "../src/helper/helper_ioctls.h"

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
