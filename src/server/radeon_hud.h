/*
 * radeon_hud.h - Herramienta de Calidad de Vida (QoL) tipo MangoHud para Haiku
 */
#ifndef RADEON_HUD_H
#define RADEON_HUD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct radeon_hud_stats {
	uint32_t fps;
	uint64_t vram_used_mb;
	uint64_t vram_total_mb;
	float gpu_temperature_c;
	uint32_t active_teams;
};

/* Renderiza o exporta las estadísticas del HUD en tiempo de ejecución */
void radeon_hud_update(const struct radeon_hud_stats *stats);
void radeon_hud_render_overlay(void *surface_buffer, int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* RADEON_HUD_H */
