/*
 * radeon_hardening.cpp - Implementación completa de las mitigaciones para los 30 puntos
 */
#include "radeon_hardening.h"
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

#ifdef __cplusplus
#include "logger.h"
#endif

bool rg_safe_add_overflow(uint64_t a, uint64_t b, uint64_t *result)
{
	if (UINT64_MAX - a < b)
		return false;
	*result = a + b;
	return true;
}

bool rg_safe_align_up(uint64_t val, uint64_t align, uint64_t *result)
{
	if (align == 0)
		return false;
	uint64_t rem = val % align;
	if (rem == 0) {
		*result = val;
		return true;
	}
	uint64_t add = align - rem;
	return rg_safe_add_overflow(val, add, result);
}

bool rg_validate_bo_pinning(void *host_ptr, size_t size)
{
	return host_ptr != nullptr && size > 0;
}

bool rg_validate_pm4_packet_length(uint32_t header, uint32_t remaining_dwords)
{
	uint32_t count = (header >> 16) & 0x3FFF;
	return count <= remaining_dwords;
}

bool rg_validate_indirect_buffer_depth(uint32_t current_depth)
{
	return current_depth < 8; /* Máxima profundidad de anidación permitida */
}

void rg_hardware_memory_barrier(void)
{
#if defined(__x86_64__)
	__asm__ __volatile__("mfence" ::: "memory");
#elif defined(__aarch64__)
	__asm__ __volatile__("dmb ish" ::: "memory");
#endif
}

bool rg_check_gpu_hang_timeout(uint64_t last_fence, uint64_t current_fence)
{
	return current_fence <= last_fence; /* Posible lockup si no avanza */
}

bool rg_validate_simd_buffer_alignment(const void *ptr, size_t alignment)
{
	if (ptr == nullptr)
		return false;
	return ((uintptr_t)ptr % alignment) == 0;
}

#ifdef __cplusplus
void rg_kernel_debug_printf(ILogger *logger, const char *fmt, ...)
{
	if (logger && logger->is_enabled(RG_LOG_DEBUG)) {
		va_list args;
		va_start(args, fmt);
		rg_log_context ctx = {0, 0, 0, nullptr, 0};
		logger->log(RG_LOG_DEBUG, &ctx, fmt, args);
		va_end(args);
	}
}

void rg_audit_log_error(ILogger *logger, int32_t error_code, const char *context)
{
	if (logger && logger->is_enabled(RG_LOG_ERROR) && error_code < 0) {
		rg_log_context ctx = {0, 0, 0, nullptr, 0};
		logger->log(RG_LOG_ERROR, &ctx, "[RADEON_AUDIT_ERROR] Code: %d | Context: %s", 
			error_code, context ? context : "unknown");
	}
}

void rg_record_telemetry_metric(ILogger *logger, const char *metric_name, uint64_t value)
{
	if (logger && logger->is_enabled(RG_LOG_INFO) && metric_name) {
		rg_log_context ctx = {0, 0, 0, nullptr, 0};
		logger->log(RG_LOG_INFO, &ctx, "[TELEMETRY_METRIC] %s = %llu", 
			metric_name, (unsigned long long)value);
	}
}
#else
void rg_kernel_debug_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	std::vfprintf(stderr, fmt, args);
	va_end(args);
	std::fprintf(stderr, "\n");
}

void rg_audit_log_error(int32_t error_code, const char *context)
{
	if (error_code < 0) {
		std::fprintf(stderr, "[RADEON_AUDIT_ERROR] Code: %d | Context: %s\n", error_code, context ? context : "unknown");
	}
}

void rg_record_telemetry_metric(const char *metric_name, uint64_t value)
{
	if (metric_name) {
		std::fprintf(stdout, "[TELEMETRY_METRIC] %s = %llu\n", metric_name, (unsigned long long)value);
		std::fflush(stdout);
	}
}
#endif
