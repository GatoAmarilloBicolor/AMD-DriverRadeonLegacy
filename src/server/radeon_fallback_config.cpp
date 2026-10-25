/*
 * radeon_fallback_config.cpp - Implementación del selector opcional de fallbacks
 */
#include "radeon_fallback_config.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

static enum radeon_backend_mode g_active_mode = RADEON_MODE_AUTO;

void radeon_init_fallback_config(void)
{
	const char *env = std::getenv("RADEON_BACKEND_MODE");
	if (env != nullptr) {
		if (std::strcmp(env, "gcn") == 0) {
			g_active_mode = RADEON_MODE_NATIVE_GCN;
			std::fprintf(stdout, "[RadeonConfig] Backend forzado a: GCN Nativo (RADV).\n");
		} else if (std::strcmp(env, "terakan") == 0) {
			g_active_mode = RADEON_MODE_TERAKAN_EMUL;
			std::fprintf(stdout, "[RadeonConfig] Backend forzado a: Terakan / TeraScale VLIW5 Emulación.\n");
		} else if (std::strcmp(env, "cpu") == 0 || std::strcmp(env, "cpu-only") == 0) {
			g_active_mode = RADEON_MODE_CPU_ONLY;
			std::fprintf(stdout, "[RadeonConfig] Backend forzado a: CPU Software (Oráculo / LLVMPIPE).\n");
		} else {
			g_active_mode = RADEON_MODE_AUTO;
			std::fprintf(stdout, "[RadeonConfig] Backend automático (Detección por familia ASIC).\n");
		}
	} else {
		g_active_mode = RADEON_MODE_AUTO;
	}
}

enum radeon_backend_mode radeon_get_active_backend_mode(void)
{
	return g_active_mode;
}
