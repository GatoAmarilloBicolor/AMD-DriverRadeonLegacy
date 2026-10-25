/*
 * xess_scaler.h - Integración del escalador compatible Intel XeSS (DP4a / Espacial) open-source
 */
#ifndef XESS_SCALER_H
#define XESS_SCALER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct xess_config {
	uint32_t width;
	uint32_t height;
	uint32_t target_width;
	uint32_t target_height;
	bool use_dp4a_acceleration;
};

/* Ejecuta el upscaling compatible con XeSS utilizando instrucciones vectoriales SIMD o DP4a simulado */
bool xess_apply_scaling(const struct xess_config *config, const float *input_rgb, float *output_rgb);

#ifdef __cplusplus
}
#endif

#endif /* XESS_SCALER_H */
