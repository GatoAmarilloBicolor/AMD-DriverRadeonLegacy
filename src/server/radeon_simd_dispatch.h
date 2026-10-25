/*
 * radeon_simd_dispatch.h - Despacho SIMD optimizado con caché estática (init-time dispatch)
 * Resuelve el punto 14 de la auditoría técnica.
 */
#ifndef RADEON_SIMD_DISPATCH_H
#define RADEON_SIMD_DISPATCH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief Inicializa y detecta las capacidades SIMD de la CPU una sola vez en el arranque.
 */
void radeon_init_simd_dispatch(void);

/*!
 * \brief Indica si AVX2 está disponible y cacheado.
 */
bool radeon_has_avx2(void);

/*!
 * \brief Indica si NEON está disponible y cacheado.
 */
bool radeon_has_neon(void);

#ifdef __cplusplus
}
#endif

#endif /* RADEON_SIMD_DISPATCH_H */
