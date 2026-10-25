/*
 * radeon_fallback_config.h - Configuración opcional y dinámica de fallbacks (GCN, Terakan/TeraScale, CPU Oracle)
 */
#ifndef RADEON_FALLBACK_CONFIG_H
#define RADEON_FALLBACK_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

enum radeon_backend_mode {
	RADEON_MODE_AUTO         = 0, /* Selección automática según familia ASIC */
	RADEON_MODE_NATIVE_GCN   = 1, /* Forzar RADV / amdgpu nativo */
	RADEON_MODE_TERAKAN_EMUL = 2, /* Forzar traducción Terakan para TeraScale VLIW5 */
	RADEON_MODE_CPU_ONLY     = 3  /* Fallback puramente por CPU (Lavapipe / llvmpipe) */
};

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa y lee las variables de entorno de fallback (ej. RADEON_BACKEND_MODE=terakan) */
void radeon_init_fallback_config(void);

/* Obtiene el modo de backend activo */
enum radeon_backend_mode radeon_get_active_backend_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* RADEON_FALLBACK_CONFIG_H */
