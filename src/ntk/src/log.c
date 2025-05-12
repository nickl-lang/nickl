#include "ntk/log.h"

#include <stdlib.h>

#include "ntk/common.h"
#include "ntk/file.h"
#include "ntk/profiler.h"
#include "ntk/stream.h"
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

typedef struct {
    NkStream out;
    i64 start_time;
    NkLogLevel log_level;
    NkHandle mtx;
    usize msg_count;
    bool initialized;
    bool to_color;
} LoggerState;

static LoggerState s_logger;

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
    NK_PROF_FUNC() {
        va_list ap;
        va_start(ap, fmt);
        nk_log_vwrite(log_level, scope, fmt, ap);
        va_end(ap);
    }
}

void nk_log_vwrite(NkLogLevel log_level, char const *scope, char const *fmt, va_list ap) {
    NkStream const out = nk_log_streamOpen(log_level, scope);
    nk_stream_vprintf(out, fmt, ap);
    nk_log_streamClose(out);
}

NkStream nk_log_streamOpen(NkLogLevel log_level, char const *scope) {
    i64 const now = nk_now_ns();
    f64 const ts = (now - s_logger.start_time) * 1e-9;

    NkStream const out = s_logger.out;

    nk_mutex_lock(s_logger.mtx);

    if (s_logger.to_color) {
        nk_stream_printf(out, NK_TERM_COLOR_NONE "%s", c_color_map[log_level]);
    }

    nk_stream_printf(out, "%04zu %lf %s %s ", ++s_logger.msg_count, ts, c_log_level_map[log_level], scope);

    return out;
}

void nk_log_streamClose(NkStream out) {
    if (s_logger.to_color) {
        nk_stream_printf(out, NK_TERM_COLOR_NONE);
    }

    nk_stream_printf(out, "\n");

    nk_mutex_unlock(s_logger.mtx);
}

void nk_log_injectStream(NkStream out) {
    nk_mutex_lock(s_logger.mtx);
    s_logger.out = out;
    nk_mutex_unlock(s_logger.mtx);
}

void nk_log_init(NkLogOptions opt) {
    NK_PROF_FUNC() {
        char const *env_log_level = getenv(ENV_VAR);
        s_logger = (LoggerState){
            .out = nk_file_getStream(nk_stderr()),
            .start_time = nk_now_ns(),
            .log_level = env_log_level ? parseEnvLogLevel(env_log_level) : opt.log_level,
            .mtx = nk_mutex_alloc(),
            .to_color =
                opt.color_mode == NkLogColorMode_Always || (opt.color_mode == NkLogColorMode_Auto && nk_isatty(2)),
            .initialized = true,
        };
    }
}
