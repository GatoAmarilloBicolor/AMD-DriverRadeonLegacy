/*
 * r600_streamout_compute.cpp - Implementación de Transform Feedback para Cómputo en R600
 */
#include "r600_streamout_compute.h"
#include <cstring>

bool r600_compile_compute_to_streamout(const struct r600_compute_pipeline *pipeline, uint32_t *out_pm4_buffer, uint32_t *out_dwords)
{
	if (pipeline == nullptr || out_pm4_buffer == nullptr || out_dwords == nullptr)
		return false;

	/* Generación de secuencia PM4 para configurar VS (Vertex Shader) como Compute y activar Stream-Out */
	uint32_t dwords = 0;
	
	out_pm4_buffer[dwords++] = 0xC0001000; /* PM4 Type 3: NOP / Packet header de configuración */
	out_pm4_buffer[dwords++] = 0x00000000;
	
	/* Configuración de workgroups mapeados a primitivas de vértices */
	out_pm4_buffer[dwords++] = 0xC0003001; /* SET_CONFIG_REG simulado para VLIW5 */
	out_pm4_buffer[dwords++] = pipeline->workgroup_size[0];

	*out_dwords = dwords;
	return true;
}
