/*
 * logger.cpp - Implementación del logger por defecto.
 */
#include "logger.h"

#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <chrono>
#include <ctime>

#ifndef __HAIKU__
#include <sys/time.h>
#endif

DefaultLogger::DefaultLogger(rg_log_level min) : fMinLevel(min) {}

void DefaultLogger::log(rg_log_level level, const rg_log_context *ctx,
	const char *fmt, ...)
{
	if (level < fMinLevel)
		return;
	
	std::lock_guard<std::mutex> guard(fMutex);
	
	// Timestamp
	auto now = std::chrono::system_clock::now();
	auto now_time_t = std::chrono::system_clock::to_time_t(now);
	auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()) % 1000;
	
	char time_buf[32];
	std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", std::localtime(&now_time_t));
	
	// Level prefix
	const char *level_str = "";
	switch (level) {
		case RG_LOG_DEBUG: level_str = "DBG"; break;
		case RG_LOG_INFO:  level_str = "INF"; break;
		case RG_LOG_WARN:  level_str = "WRN"; break;
		case RG_LOG_ERROR: level_str = "ERR"; break;
	}
	
	// Print prefix
	std::fprintf(stderr, "[%s.%03d] [%s] ", time_buf, static_cast<int>(now_ms.count()), level_str);
	
	// Print context if present
	if (ctx) {
		if (ctx->team_id)
			std::fprintf(stderr, "[team=%u] ", ctx->team_id);
		if (ctx->fence)
			std::fprintf(stderr, "[fence=%llu] ", (unsigned long long)ctx->fence);
		if (ctx->bo_handle)
			std::fprintf(stderr, "[bo=%u] ", ctx->bo_handle);
	}
	
	// Print message
	va_list args;
	va_start(args, fmt);
	std::vfprintf(stderr, fmt, args);
	va_end(args);
	
	std::fputc('\n', stderr);
}

bool DefaultLogger::is_enabled(rg_log_level level) const
{
	return level >= fMinLevel;
}

void DefaultLogger::set_min_level(rg_log_level level)
{
	std::lock_guard<std::mutex> guard(fMutex);
	fMinLevel = level;
}

rg_log_level DefaultLogger::min_level() const
{
	return fMinLevel;
}