#ifdef ENABLE_LOGGING

#include "ntk/logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntk/sys/term.h"
#include "ntk/sys/thread.h"
#include "ntk/sys/time.h"

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

struct LoggerState {
    nktime_t start_time;
    NkLogLevel log_level;
    NkColorMode color_mode;
    nk_mutex_t mtx;
    size_t msg_count;
    bool initialized;
};

static struct LoggerState s_logger;

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
    bool const to_color = s_logger.color_mode == NkLog_Color_Always ||
                          (s_logger.color_mode == NkLog_Color_Auto && nk_isatty(STDERR_FILENO));

    nktime_t now = nk_getTimeNs();
    double ts = (now - s_logger.start_time) / 1e9;

    nk_mutex_lock(&s_logger.mtx);

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

    nk_mutex_unlock(&s_logger.mtx);
}

void _nk_loggerInit(NkLoggerOptions opt) {
    s_logger = (struct LoggerState){0};

    s_logger.start_time = nk_getTimeNs();

    s_logger.log_level = opt.log_level;
    s_logger.color_mode = opt.color_mode;

    nk_mutex_init(&s_logger.mtx);

    s_logger.initialized = true;
}

#endif // ENABLE_LOGGING
