/*
 * dual_gpu_switcher.cpp - Implementación del gestor de gráficos duales combinados
 */
#include "dual_gpu_switcher.h"
#include <cstdio>

static enum dual_gpu_mode g_current_gpu_mode = DUAL_GPU_MODE_INTEGRATED_ONLY;

bool dual_gpu_switcher_init(enum dual_gpu_mode initial_mode)
{
	g_current_gpu_mode = initial_mode;
	std::fprintf(stdout, "[DualGPU] Inicializador de Gráficos Híbridos / Switchable Graphics activo (Modo inicial: %d).\n", (int)initial_mode);
	return true;
}

bool dual_gpu_switch_context(enum dual_gpu_mode target_mode)
{
	if (g_current_gpu_mode == target_mode)
	{
		return true;
	}

	g_current_gpu_mode = target_mode;
	std::fprintf(stdout, "[DualGPU] Conmutando contexto gráfico a modo: %d (Integrada / Dedicada / Híbrida).\n", (int)target_mode);
	return true;
}
