/*
 * super_resolution.h - Suite unificada de técnicas Open Source de Super Resolución (FSR 1.0, FSR 2.x Temporal y Integer Scaling)
 */
#ifndef SUPER_RESOLUTION_H
#define SUPER_RESOLUTION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum sr_algorithm {
	SR_ALG_INTEGER_SCALING = 1,
	SR_ALG_FSR_1_SPATIAL   = 2,
	SR_ALG_FSR_2_TEMPORAL  = 3
};

struct sr_context {
	enum sr_algorithm algorithm;
	uint32_t width;
	uint32_t height;
	uint32_t target_width;
	uint32_t target_height;
	float sharpness;
};

/* Inicializa y ejecuta la técnica de super resolución seleccionada */
bool sr_apply_upscale(const struct sr_context *ctx, const float *input_color, float *output_color);

#ifdef __cplusplus
}
#endif

#endif /* SUPER_RESOLUTION_H */
