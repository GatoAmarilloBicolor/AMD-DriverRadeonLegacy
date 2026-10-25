/*
 * app_server_2d.h - Integración con app_server y aceleración 2D / Deskbar Replicant (§Fase 10)
 */
#ifndef APP_SERVER_2D_H
#define APP_SERVER_2D_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hooks de aceleración 2D para el accelerant de radeon_hd / app_server */
int32_t radeon_accelerant_fill_rect(uint32_t color, uint32_t num_rects, void *rects);
int32_t radeon_accelerant_screen_blit(uint32_t xsrc, uint32_t ysrc, uint32_t xdst, uint32_t ydst, uint32_t w, uint32_t h);

/* Replicant / Deskbar add-on para monitoreo de estado de GPU y VRAM */
void radeon_deskbar_replicant_update(uint64_t vram_used, uint64_t vram_total, float temperature);

#ifdef __cplusplus
}
#endif

#endif /* APP_SERVER_2D_H */
