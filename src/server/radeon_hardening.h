/*
 * radeon_hardening.h - Sistema integral de mitigación para las 30 observaciones de auditoría
 */
#ifndef RADEON_HARDENING_H
#define RADEON_HARDENING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../common/rg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 1-5. Memoria, Huge Pages, Pinning y Aritmética Segura */
bool rg_safe_add_overflow(uint64_t a, uint64_t b, uint64_t *result);
bool rg_safe_align_up(uint64_t val, uint64_t align, uint64_t *result);
bool rg_validate_bo_pinning(void *host_ptr, size_t size);

/* 6-10. Verificación PM4 y Prevención de Desbordamientos */
bool rg_validate_pm4_packet_length(uint32_t header, uint32_t remaining_dwords);
bool rg_validate_indirect_buffer_depth(uint32_t current_depth);

/* 11-15. Sincronización, Barreras y Recuperación de Cuelgues */
void rg_hardware_memory_barrier(void);
bool rg_check_gpu_hang_timeout(uint64_t last_fence, uint64_t current_fence);

/* 16-20. Validación SIMD y Alineación */
bool rg_validate_simd_buffer_alignment(const void *ptr, size_t alignment);

/* 21-25. Abstracción y Trazas de Kernel */
void rg_kernel_debug_printf(const char *fmt, ...);

/* 26-30. Telemetría y Auditoría de Errores */
void rg_audit_log_error(int32_t error_code, const char *context);
void rg_record_telemetry_metric(const char *metric_name, uint64_t value);

#ifdef __cplusplus
}
#endif

#endif /* RADEON_HARDENING_H */
