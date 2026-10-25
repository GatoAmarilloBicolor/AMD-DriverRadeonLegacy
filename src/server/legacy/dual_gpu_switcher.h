/*
 * dual_gpu_switcher.h - Gestión de Gráficos Duales Combinados / Switchable Graphics (PowerXpress / Enduro)
 */
#ifndef DUAL_GPU_SWITCHER_H
#define DUAL_GPU_SWITCHER_H

#include <stdint.h>
#include <stdbool.h>

enum dual_gpu_mode {
	DUAL_GPU_MODE_INTEGRATED_ONLY = 1, /* Ahorro de energía (APU Radeon HD 6290/8250) */
	DUAL_GPU_MODE_DISCRETE_ONLY   = 2, /* Alto rendimiento (GPU dedicada Radeon HD 8750M) */
	DUAL_GPU_MODE_HYBRID_CROSSFIRE= 3  /* Rendimiento combinado en paralelo (CrossFire híbrido) */
};

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa el selector de gráficos duales combinados */
bool dual_gpu_switcher_init(enum dual_gpu_mode initial_mode);

/* Conmuta el contexto de renderizado dinámicamente entre integrada y dedicada */
bool dual_gpu_switch_context(enum dual_gpu_mode target_mode);

#ifdef __cplusplus
}
#endif

#endif /* DUAL_GPU_SWITCHER_H */
