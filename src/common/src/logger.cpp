#ifdef ENABLE_LOGGING

#include "nk/common/logger.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "nk/sys/term.h"

int fileno(FILE *);

#define ENV_VAR "NK_LOG_LEVEL"

static char const *c_color_map[] = {
    NULL,                  // None
    NK_TERM_COLOR_RED,     // Error
    NK_TERM_COLOR_YELLOW,  // Warning
    NK_TERM_COLOR_BLUE,    // Info
    NK_TERM_COLOR_GREEN,   // Debug
    NK_TERM_COLOR_MAGENTA, // Trace
};

static char const *c_log_level_map[] = {
    NULL,
    "error",
    "warning",
    "info",
    "debug",
    "trace",
};

static char const *c_env_log_level_map[] = {
    "none",
    "error",
    "warning",
    "info",
    "debug",
    "trace",
};

typedef struct {
    std::chrono::steady_clock::time_point start_time;
    NkLogLevel log_level;
    NkColorMode color_mode;
    std::mutex mutex;
    size_t msg_count;
    bool initialized;
} LoggerState;

static LoggerState s_logger;

static NkLogLevel parseEnvLogLevel(char const *env_log_level) {
    size_t i = 0;
    for (; i <= NkLog_Trace; i++) {
        if (strcmp(env_log_level, c_env_log_level_map[i]) == 0) {
            return (NkLogLevel)i;
        }
    }
    return NkLog_None;
}

bool _nk_loggerCheck(NkLogLevel log_level) {
    char const *env_log_level = getenv(ENV_VAR);
    return s_logger.initialized && log_level <= (env_log_level ? parseEnvLogLevel(env_log_level) : s_logger.log_level);
}

void _nk_loggerWrite(NkLogLevel log_level, char const *scope, char const *fmt, ...) {
    bool const to_color =
        s_logger.color_mode == NkLog_Color_Always || (s_logger.color_mode == NkLog_Color_Auto && nk_isatty());

    auto now = std::chrono::steady_clock::now();
    auto ts = std::chrono::duration<double>{now - s_logger.start_time}.count();

    std::lock_guard<std::mutex> lk{s_logger.mutex};

    if (to_color) {
        fprintf(stderr, NK_TERM_COLOR_NONE "%s", c_color_map[log_level]);
    }

    fprintf(stderr, "%04zu %lf %s %s ", ++s_logger.msg_count, ts, c_log_level_map[log_level], scope);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (to_color) {
        fprintf(stderr, NK_TERM_COLOR_NONE);
    }

    fputc('\n', stderr);
}

void _nk_loggerInit(NkLoggerOptions opt) {
    s_logger.start_time = std::chrono::steady_clock::now();

    s_logger.log_level = opt.log_level;
    s_logger.color_mode = opt.color_mode;

    s_logger.initialized = true;
}

#endif // ENABLE_LOGGING
