/*
 * lvp_exec_cpu.h - Ejecutor CPU y oraculo (C5 / §8.7).
 */
#ifndef RG_LVP_EXEC_CPU_H
#define RG_LVP_EXEC_CPU_H

#include <stdint.h>
#include <stddef.h>

/* Relleno de color RGBA8888 (operacion minima verificable). */
void rg_cpu_clear_rgba(uint8_t *dst, size_t pixels, uint32_t rgba);

/* Copia de buffer con conversion opcional de formato (oraculo). */
void rg_cpu_copy(uint8_t *dst, const uint8_t *src, size_t bytes);

/* Compara dos buffers; devuelve 0 si son iguales, o el primer offset distinto. */
size_t rg_cpu_compare(const uint8_t *a, const uint8_t *b, size_t bytes);

#endif /* RG_LVP_EXEC_CPU_H */
