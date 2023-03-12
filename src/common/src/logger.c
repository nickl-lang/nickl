#ifdef ENABLE_LOGGING

#include "nk/common/logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "nk/sys/term.h"

int fileno(FILE *);

#define ENV_VAR "NK_LOG_LEVEL"

static char const *c_color_map[] = {
    NULL,               // None
    TERM_COLOR_RED,     // Error
    TERM_COLOR_YELLOW,  // Warning
    TERM_COLOR_BLUE,    // Info
    TERM_COLOR_GREEN,   // Debug
    TERM_COLOR_MAGENTA, // Trace
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
    struct timespec start_time;
    NkLogLevel log_level;
    NkColorMode color_mode;
    mtx_t mutex;
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
    return s_logger.initialized &&
           log_level <= (env_log_level ? parseEnvLogLevel(env_log_level) : s_logger.log_level);
}

void _nk_loggerWrite(NkLogLevel log_level, char const *scope, char const *fmt, ...) {
    bool const to_color = s_logger.color_mode == NkLog_Color_Always ||
                          (s_logger.color_mode == NkLog_Color_Auto && nksys_isatty());

    struct timespec now;
    timespec_get(&now, TIME_UTC);
    double ts =
        now.tv_sec - s_logger.start_time.tv_sec + (now.tv_nsec - s_logger.start_time.tv_nsec) / 1e9;

    mtx_lock(&s_logger.mutex);

    if (to_color) {
        fprintf(stderr, TERM_COLOR_NONE "%s", c_color_map[log_level]);
    }

    fprintf(
        stderr, "%04zu %lf %s %s ", ++s_logger.msg_count, ts, c_log_level_map[log_level], scope);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (to_color) {
        fprintf(stderr, TERM_COLOR_NONE);
    }

    fputc('\n', stderr);

    mtx_unlock(&s_logger.mutex);
}

void _nk_loggerInit(NkLoggerOptions opt) {
    s_logger = (LoggerState){0};

    timespec_get(&s_logger.start_time, TIME_UTC);

    s_logger.log_level = opt.log_level;
    s_logger.color_mode = opt.color_mode;

    mtx_init(&s_logger.mutex, mtx_plain);

    s_logger.initialized = true;
}

#endif // ENABLE_LOGGING
