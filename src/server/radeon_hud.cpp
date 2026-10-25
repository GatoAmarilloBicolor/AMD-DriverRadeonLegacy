/*
 * radeon_hud.cpp - Implementación del HUD de rendimiento QoL
 */
#include "radeon_hud.h"
#include <cstdio>

void radeon_hud_update(const struct radeon_hud_stats *stats)
{
	if (stats == nullptr)
		return;
	
	std::fprintf(stdout, "[RadeonHUD] FPS: %u | VRAM: %lluMB / %lluMB | Temp: %.1f°C | Teams: %u\n",
		stats->fps,
		(unsigned long long)stats->vram_used_mb,
		(unsigned long long)stats->vram_total_mb,
		stats->gpu_temperature_c,
		stats->active_teams);
	std::fflush(stdout);
}

void radeon_hud_render_overlay(void *surface_buffer, int width, int height)
{
	if (surface_buffer == nullptr || width <= 0 || height <= 0)
		return;
	
	/* Renderizado conceptual del HUD sobre el buffer de presentación de la WSI */
}
