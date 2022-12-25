#ifdef ENABLE_LOGGING

#include "nk/utils/logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

int fileno(FILE *);

#define ENV_VAR "NK_LOG_LEVEL"

#define COLOR_NONE "\x1b[0m"
// #define COLOR_GRAY "\x1b[1;30m"
#define COLOR_RED "\x1b[1;31m"
#define COLOR_GREEN "\x1b[1;32m"
#define COLOR_YELLOW "\x1b[1;33m"
#define COLOR_BLUE "\x1b[1;34m"
#define COLOR_MAGENTA "\x1b[1;35m"
// #define COLOR_CYAN "\x1b[1;36m"
// #define COLOR_WHITE "\x1b[1;37m"

char const *c_color_map[] = {
    NULL,          // None
    COLOR_RED,     // Error
    COLOR_YELLOW,  // Warning
    COLOR_BLUE,    // Info
    COLOR_GREEN,   // Debug
    COLOR_MAGENTA, // Trace
};

char const *c_log_level_map[] = {
    NULL,
    "error",
    "warning",
    "info",
    "debug",
    "trace",
};

char const *c_env_log_level_map[] = {
    "none",
    "error",
    "warning",
    "info",
    "debug",
    "trace",
};

typedef struct {
    struct timespec start_time;
    ELogLevel log_level;
    EColorMode color_mode;
    mtx_t mutex;
    size_t msg_count;
    bool initialized;
} LoggerState;

static LoggerState s_logger;

ELogLevel parseEnvLogLevel(char const *env_log_level) {
    size_t i = 0;
    for (; i <= Log_Trace; i++) {
        if (strcmp(env_log_level, c_env_log_level_map[i]) == 0) {
            return (ELogLevel)i;
        }
    }
    return Log_None;
}

bool _logger_check(ELogLevel log_level) {
    char const *env_log_level = getenv(ENV_VAR);
    return s_logger.initialized &&
           log_level <= (env_log_level ? parseEnvLogLevel(env_log_level) : s_logger.log_level);
}

void _logger_write(ELogLevel log_level, char const *scope, char const *fmt, ...) {
    bool const to_color = s_logger.color_mode == Log_Color_Always ||
                          (s_logger.color_mode == Log_Color_Auto && isatty(fileno(stdout)));

    struct timespec now;
    timespec_get(&now, TIME_UTC);
    double ts =
        now.tv_sec - s_logger.start_time.tv_sec + (now.tv_nsec - s_logger.start_time.tv_nsec) / 1e9;

    mtx_lock(&s_logger.mutex);

    if (to_color) {
        fprintf(stderr, COLOR_NONE "%s", c_color_map[log_level]);
    }

    fprintf(
        stderr, "%04zu %lf %s %s ", ++s_logger.msg_count, ts, c_log_level_map[log_level], scope);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (to_color) {
        fprintf(stderr, COLOR_NONE);
    }

    fputc('\n', stderr);

    mtx_unlock(&s_logger.mutex);
}

void _logger_init(LoggerOptions opt) {
    s_logger = (LoggerState){0};

    timespec_get(&s_logger.start_time, TIME_UTC);

    s_logger.log_level = opt.log_level;
    s_logger.color_mode = opt.color_mode;

    mtx_init(&s_logger.mutex, mtx_plain);

    s_logger.initialized = true;
}

#endif // ENABLE_LOGGING
