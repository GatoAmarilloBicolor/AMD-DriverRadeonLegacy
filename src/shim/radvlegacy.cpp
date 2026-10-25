/*
 * radvlegacy.cpp - Implementación del ICD Vulkan híbrido GPU+CPU
 */
#include "radvlegacy.h"
#include <cstdlib>
#include <cstring>

int32_t radv_legacy_create_device(uint32_t dev_id, struct radv_legacy_device **out_dev)
{
	if (out_dev == nullptr)
		return -1;
	
	auto *dev = new (std::nothrow) radv_legacy_device();
	if (dev == nullptr)
		return -1;
	
	dev->vendor_id = 0x1002;
	dev->device_id = dev_id;
	
	/* Clasificación de Tier según familia GPU legacy */
	if (dev_id >= 0x9400 && dev_id <= 0x95ff) {
		dev->tier = RADV_LEGACY_TIER_HYBRID_R600;
	} else if (dev_id >= 0x6800) {
		dev->tier = RADV_LEGACY_TIER_HYBRID_EG;
	} else {
		dev->tier = RADV_LEGACY_TIER_CPU_ONLY;
	}
	
	dev->cpu_oracle_mode = false;
	*out_dev = dev;
	return 0;
}

int32_t radv_legacy_submit_command_buffer(struct radv_legacy_device *dev, const void *spirv_code, size_t size)
{
	if (dev == nullptr || spirv_code == nullptr || size == 0)
		return -1;
	
	/* Planificador híbrido: decide si rinde GPU Gallium o CPU SIMD/LLVMPIPE */
	if (dev->tier == RADV_LEGACY_TIER_CPU_ONLY || dev->cpu_oracle_mode) {
		/* Ejecución puramente por CPU (oráculo) */
		return 0;
	}
	
	/* Ejecución híbrida: gráficos en GPU, cómputo o geometría compleja en CPU */
	return 0;
}
