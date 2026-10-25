/*
 * logger.h - Interfaz de logging estructurado (C2 / §6.12).
 *
 * Sustituye fprintf/dprintf por niveles, contexto y testabilidad.
 */
#ifndef RG_LOGGER_H
#define RG_LOGGER_H

#include <stdint.h>
#include <cstdarg>
#include <mutex>

enum rg_log_level {
	RG_LOG_DEBUG = 0,
	RG_LOG_INFO  = 1,
	RG_LOG_WARN  = 2,
	RG_LOG_ERROR = 3
};

struct rg_log_context {
	uint32_t team_id;
	uint64_t fence;
	uint32_t bo_handle;
	const char *file;
	int line;
};

class ILogger {
public:
	virtual ~ILogger() = default;
	
	/* Log a message with optional context */
	virtual void log(rg_log_level level, const rg_log_context *ctx,
		const char *fmt, ...) = 0;
	
	/* Check if level is enabled (avoids formatting overhead) */
	virtual bool is_enabled(rg_log_level level) const = 0;
	
	/* Set minimum log level */
	virtual void set_min_level(rg_log_level level) = 0;
	
	/* Get current minimum log level */
	virtual rg_log_level min_level() const = 0;
};

/* Convenience macros for logging with automatic context */
#define RG_LOG(logger, level, ...) \
	do { \
		if ((logger) && (logger)->is_enabled(level)) { \
			rg_log_context _ctx = {0, 0, 0, __FILE__, __LINE__}; \
			(logger)->log(level, &_ctx, __VA_ARGS__); \
		} \
	} while (0)

#define RG_LOG_CTX(logger, level, team, fence, bo, ...) \
	do { \
		if ((logger) && (logger)->is_enabled(level)) { \
			rg_log_context _ctx = {(team), (fence), (bo), __FILE__, __LINE__}; \
			(logger)->log(level, &_ctx, __VA_ARGS__); \
		} \
	} while (0)

/* Thread-safe default logger implementation (host/Haiku) */
class DefaultLogger : public ILogger {
public:
	explicit DefaultLogger(rg_log_level min = RG_LOG_INFO);
	
	void log(rg_log_level level, const rg_log_context *ctx,
		const char *fmt, ...) override;
	bool is_enabled(rg_log_level level) const override;
	void set_min_level(rg_log_level level) override;
	rg_log_level min_level() const override;
	
private:
	rg_log_level fMinLevel;
	mutable std::mutex fMutex;
};

#endif /* RG_LOGGER_H */