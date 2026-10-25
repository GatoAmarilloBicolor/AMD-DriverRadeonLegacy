/*
 * hardware_resquicios.h - Explotación de características avanzadas y ocultas (Dual Graphics, Resizable BAR, Headless Mode)
 */
#ifndef HARDWARE_RESQUICIOS_H
#define HARDWARE_RESQUICIOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum headless_mode {
	HEADLESS_DISABLED = 0,
	HEADLESS_TRANSCODE_NODE = 1,
	HEADLESS_COMPUTE_CLUSTER = 2
};

/* Configura el balanceo de carga para Gráficos Duales / CrossFire Asimétrico (60% iGPU / 40% dGPU) */
bool hw_resquicios_configure_asymmetric_crossfire(bool enable, float igpu_ratio);

/* Habilita el mapeo completo de Resizable BAR (SmartAccess Memory) en Vulkan para acceso CPU directo (memcpy) */
bool hw_resquicios_enable_resizable_bar(void *device_memory_handle, size_t bar_size_bytes);

/* Transforma el hardware en un Nodo Headless de alta eficiencia para transcodificación o cómputo */
bool hw_resquicios_set_headless_mode(enum headless_mode mode);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_RESQUICIOS_H */
