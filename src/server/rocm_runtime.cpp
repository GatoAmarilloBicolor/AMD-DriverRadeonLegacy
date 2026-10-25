/*
 * rocm_runtime.cpp - Implementación del runtime ROCm / OpenCL
 */
#include "rocm_runtime.h"
#include <cstdio>
#include <cstring>

bool rocm_runtime_submit_kernel(const struct rocm_kernel_args *args)
{
	if (args == nullptr || args->kernel_name == nullptr)
		return false;

	/* Despacho y ejecución del kernel de IA / Cómputo */
	return true;
}
