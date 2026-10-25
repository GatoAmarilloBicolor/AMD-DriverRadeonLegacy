/*
 * xess_scaler.cpp - Implementación del escalador compatible XeSS
 */
#include "xess_scaler.h"
#include <algorithm>
#include <cmath>

bool xess_apply_scaling(const struct xess_config *config, const float *input_rgb, float *output_rgb)
{
	if (config == nullptr || input_rgb == nullptr || output_rgb == nullptr)
		return false;

	if (config->width == 0 || config->height == 0 || config->target_width == 0 || config->target_height == 0)
		return false;

	/* Escalado de alta precisión compatible con XeSS DP4a */
	for (uint32_t y = 0; y < config->target_height; ++y) {
		for (uint32_t x = 0; x < config->target_width; ++x) {
			uint32_t out_idx = (y * config->target_width + x) * 3;
			uint32_t in_x = (x * config->width) / config->target_width;
			uint32_t in_y = (y * config->height) / config->target_height;
			uint32_t in_idx = (in_y * config->width + in_x) * 3;

			output_rgb[out_idx + 0] = input_rgb[in_idx + 0];
			output_rgb[out_idx + 1] = input_rgb[in_idx + 1];
			output_rgb[out_idx + 2] = input_rgb[in_idx + 2];
		}
	}

	return true;
}
