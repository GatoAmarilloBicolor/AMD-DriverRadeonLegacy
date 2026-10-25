/*
 * fsr_scaler.h - Integración de AMD FidelityFX Super Resolution (FSR 1.0 Espacial) open-source
 * Optimizado para GPUs legacy con VRAM limitada.
 */
#ifndef FSR_SCALER_H
#define FSR_SCALER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct fsr_config {
	uint32_t input_width;
	uint32_t input_height;
	uint32_t output_width;
	uint32_t output_height;
	float sharpness; /* 0.0 a 2.0 */
};

/* Inicializa y ejecuta el pase de escalado espacial FSR optimizado con SIMD */
bool fsr_apply_spatial_scaling(const float *src_rgb, float *dst_rgb, const struct fsr_config *config);

#ifdef __cplusplus
}
#endif

#endif /* FSR_SCALER_H */
