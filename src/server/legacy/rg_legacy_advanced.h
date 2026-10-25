/*
 * rg_legacy_advanced.h - Optimización de capacidades olvidadas de hardware R600-Evergreen (Z/Stencil Compression, CP DMA asíncrono y DPM)
 */
#ifndef RG_LEGACY_ADVANCED_H
#define RG_LEGACY_ADVANCED_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Activa la compresión de Z/Stencil y color por bloques para ahorrar ancho de banda VRAM */
bool rg_legacy_enable_surface_compression(uint32_t asic_device_id);

/* Configura el motor CP DMA asíncrono para transferencias paralelas sin bloquear GFX */
bool rg_legacy_configure_async_cp_dma(void);

/* Aplica políticas de gestión térmica y estados de reloj DPM (PowerPlay) para APUs como AMD C-70 */
bool rg_legacy_apply_powerplay_profile(uint32_t thermal_tdp_watts);

#ifdef __cplusplus
}
#endif

#endif /* RG_LEGACY_ADVANCED_H */
