#include "ntk/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntk/common.h"
#include "ntk/profiler.h"
#include "ntk/term.h"
#include "ntk/thread.h"
#include "ntk/time.h"

#define ENV_VAR "NK_LOG_LEVEL"

_Static_assert(NkLogLevel_None == 0, "Log level enum changed");
_Static_assert(NkLogLevel_Fatal == 1, "Log level enum changed");
_Static_assert(NkLogLevel_Error == 2, "Log level enum changed");
_Static_assert(NkLogLevel_Warning == 3, "Log level enum changed");
_Static_assert(NkLogLevel_Info == 4, "Log level enum changed");
_Static_assert(NkLogLevel_Debug == 5, "Log level enum changed");
_Static_assert(NkLogLevel_Trace == 6, "Log level enum changed");

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
    NkLogColorMode color_mode;
    NkHandle mtx;
    usize msg_count;
    bool initialized;
};

static struct LoggerState s_logger;

static NkLogLevel parseEnvLogLevel(char const *env_log_level) {
    usize i = 0;
    for (; i <= NkLogLevel_Trace; i++) {
        if (strcmp(env_log_level, c_log_level_map[i]) == 0) {
            return (NkLogLevel)i;
        }
    }
    return NkLogLevel_None;
}

bool nk_log_check(NkLogLevel log_level) {
    return s_logger.initialized && log_level <= s_logger.log_level;
}

void nk_log_write(NkLogLevel log_level, char const *scope, char const *fmt, ...) {
    NK_PROF_FUNC_BEGIN();

    va_list ap;
    va_start(ap, fmt);
    nk_log_vwrite(log_level, scope, fmt, ap);
    va_end(ap);

    NK_PROF_FUNC_END();
}

void nk_log_vwrite(NkLogLevel log_level, char const *scope, char const *fmt, va_list ap) {
    bool const to_color = s_logger.color_mode == NkLogColorMode_Always ||
                          (s_logger.color_mode == NkLogColorMode_Auto && nk_isatty(STDERR_FILENO));

    i64 now = nk_now_ns();
    f64 ts = (now - s_logger.start_time) * 1e-9;

    nk_mutex_lock(s_logger.mtx);

    if (to_color) {
        fprintf(stderr, NK_TERM_COLOR_NONE "%s", c_color_map[log_level]);
    }

    fprintf(stderr, "%04zu %lf %s %s ", ++s_logger.msg_count, ts, c_log_level_map[log_level], scope);
    vfprintf(stderr, fmt, ap);

    if (to_color) {
        fprintf(stderr, NK_TERM_COLOR_NONE);
    }

    fputc('\n', stderr);

    nk_mutex_unlock(s_logger.mtx);
}

void nk_log_init(NkLogOptions opt) {
    NK_PROF_FUNC_BEGIN();

    s_logger = (struct LoggerState){0};

    s_logger.start_time = nk_now_ns();

    char const *env_log_level = getenv(ENV_VAR);
    if (env_log_level) {
        s_logger.log_level = parseEnvLogLevel(env_log_level);
    } else {
        s_logger.log_level = opt.log_level;
    }

    s_logger.color_mode = opt.color_mode;

    s_logger.mtx = nk_mutex_alloc();

    s_logger.initialized = true;

    NK_PROF_FUNC_END();
}
