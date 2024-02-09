#include "ntk/logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntk/profiler.h"
#include "ntk/sys/term.h"
#include "ntk/sys/thread.h"
#include "ntk/time.h"

#define ENV_VAR "NK_LOG_LEVEL"

static char const *c_color_map[] = {
    NULL,                  // None
    NK_TERM_COLOR_RED,     // Fatal
    NK_TERM_COLOR_RED,     // Error
    NK_TERM_COLOR_YELLOW,  // Warning
    NK_TERM_COLOR_BLUE,    // Info
    NK_TERM_COLOR_GREEN,   // Debug
    NK_TERM_COLOR_MAGENTA, // Trace
};

static char const *c_log_level_map[] = {
    NULL,
    "fatal",
    "error",
    "warning",
    "info",
    "debug",
    "trace",
};

static char const *c_env_log_level_map[] = {
    "none",
    "fatal",
    "error",
    "warning",
    "info",
    "debug",
    "trace",
};

struct LoggerState {
    i64 start_time;
    NkLogLevel log_level;
    NkColorMode color_mode;
    nk_mutex_t mtx;
    usize msg_count;
    bool initialized;
};

static struct LoggerState s_logger;

static NkLogLevel parseEnvLogLevel(char const *env_log_level) {
    usize i = 0;
    for (; i <= NkLog_Trace; i++) {
        if (strcmp(env_log_level, c_env_log_level_map[i]) == 0) {
            return (NkLogLevel)i;
        }
    }
    return NkLog_None;
}

bool nk_loggerCheck(NkLogLevel log_level) {
    return s_logger.initialized && log_level <= s_logger.log_level;
}

void nk_loggerWrite(NkLogLevel log_level, char const *scope, char const *fmt, ...) {
    ProfBeginFunc();

    va_list ap;
    va_start(ap, fmt);
    nk_vloggerWrite(log_level, scope, fmt, ap);
    va_end(ap);

    ProfEndBlock();
}

void nk_vloggerWrite(NkLogLevel log_level, char const *scope, char const *fmt, va_list ap) {
    bool const to_color = s_logger.color_mode == NkLog_Color_Always ||
                          (s_logger.color_mode == NkLog_Color_Auto && nk_isatty(STDERR_FILENO));

    i64 now = nk_getTimeNs();
    f64 ts = (now - s_logger.start_time) / 1e9;

    nk_mutex_lock(&s_logger.mtx);

    if (to_color) {
        fprintf(stderr, NK_TERM_COLOR_NONE "%s", c_color_map[log_level]);
    }

    fprintf(stderr, "%04zu %lf %s %s ", ++s_logger.msg_count, ts, c_log_level_map[log_level], scope);
    vfprintf(stderr, fmt, ap);

    if (to_color) {
        fprintf(stderr, NK_TERM_COLOR_NONE);
    }

    fputc('\n', stderr);

    nk_mutex_unlock(&s_logger.mtx);
}

void nk_loggerInit(NkLoggerOptions opt) {
    ProfBeginFunc();

    s_logger = (struct LoggerState){0};

    s_logger.start_time = nk_getTimeNs();

    char const *env_log_level = getenv(ENV_VAR);
    if (env_log_level) {
        s_logger.log_level = parseEnvLogLevel(env_log_level);
    } else {
        s_logger.log_level = opt.log_level;
    }

    s_logger.color_mode = opt.color_mode;

    nk_mutex_init(&s_logger.mtx);

    s_logger.initialized = true;

    ProfEndBlock();
}
