/*
 * radvlegacy.h - ICD Vulkan 1.0 Híbrido GPU+CPU para GPUs antiguas (R300–Northern Islands)
 * Basado en §8 de PLAN_RADEON_LEGACY_HAIKU-2.md.
 */
#ifndef RADV_LEGACY_H
#define RADV_LEGACY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum radv_legacy_tier {
	RADV_LEGACY_TIER_CPU_ONLY = 5,    /* R300-R500: Todo en CPU con oráculo */
	RADV_LEGACY_TIER_HYBRID_R600 = 4, /* R600-R700: Gráficos en GPU, cómputo en CPU */
	RADV_LEGACY_TIER_HYBRID_EG = 4    /* Evergreen/NI: Casi todo en GPU, cómputo acelerado */
};

struct radv_legacy_device {
	uint32_t vendor_id;
	uint32_t device_id;
	enum radv_legacy_tier tier;
	bool cpu_oracle_mode;            /* RG_VK_MODE=cpu-only */
};

/* Inicialización del ICD Vulkan híbrido */
int32_t radv_legacy_create_device(uint32_t dev_id, struct radv_legacy_device **out_dev);

/* Ejecución de Command Buffer híbrido (GPU o CPU según planificador lvp-hw) */
int32_t radv_legacy_submit_command_buffer(struct radv_legacy_device *dev, const void *spirv_code, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* RADV_LEGACY_H */
