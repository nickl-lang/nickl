#ifndef HEADER_GUARD_NTK_LOGGER
#define HEADER_GUARD_NTK_LOGGER

#include <stdarg.h>
#include <stdbool.h>

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_LOGGING

typedef enum {
    NkLog_None = 0,
    NkLog_Fatal,
    NkLog_Error,
    NkLog_Warning,
    NkLog_Info,
    NkLog_Debug,
    NkLog_Trace,
} NkLogLevel;

typedef enum {
    NkLog_Color_Auto = 0,
    NkLog_Color_Always,
    NkLog_Color_Never,
} NkColorMode;

typedef struct {
    NkLogLevel log_level;
    NkColorMode color_mode;
} NkLoggerOptions;

bool _nk_loggerCheck(NkLogLevel log_level);
NK_PRINTF_LIKE(3, 4) void _nk_loggerWrite(NkLogLevel log_level, char const *scope, char const *fmt, ...);

void _nk_vloggerWrite(NkLogLevel log_level, char const *scope, char const *fmt, va_list ap);

void _nk_loggerInit(NkLoggerOptions opt);

#define NK_LOG_SEV(LEVEL, ...)                                    \
    if (_nk_loggerCheck(LEVEL)) {                                 \
        _nk_loggerWrite((LEVEL), __nk_logger_scope, __VA_ARGS__); \
    } else                                                        \
        (void)0

#define NK_VLOG_SEV(LEVEL, FMT, AP)                                \
    if (_nk_loggerCheck(LEVEL)) {                                  \
        _nk_vloggerWrite((LEVEL), __nk_logger_scope, (FMT), (AP)); \
    } else                                                         \
        (void)0

#define NK_LOGGER_INIT(...) _nk_loggerInit(__VA_ARGS__)

#define NK_LOG_USE_SCOPE(NAME) static char const *__nk_logger_scope = #NAME

#define NK_LOG_FAT(...) NK_LOG_SEV(NkLog_Fatal, __VA_ARGS__)
#define NK_LOG_ERR(...) NK_LOG_SEV(NkLog_Error, __VA_ARGS__)
#define NK_LOG_WRN(...) NK_LOG_SEV(NkLog_Warning, __VA_ARGS__)
#define NK_LOG_INF(...) NK_LOG_SEV(NkLog_Info, __VA_ARGS__)
#define NK_LOG_DBG(...) NK_LOG_SEV(NkLog_Debug, __VA_ARGS__)
#define NK_LOG_TRC(...) NK_LOG_SEV(NkLog_Trace, __VA_ARGS__)

#else // ENABLE_LOGGING

#define _NK_LOG_NOP (void)0
#define _NK_LOG_NOP_TOPLEVEL extern int _

#define NK_LOG_SEV(...) _NK_LOG_NOP
#define NK_VLOG_SEV(...) _NK_LOG_NOP

#define NK_LOGGER_INIT(...) _NK_LOG_NOP

#define NK_LOG_USE_SCOPE(...) _NK_LOG_NOP_TOPLEVEL

#define NK_LOG_FAT(...) _NK_LOG_NOP
#define NK_LOG_ERR(...) _NK_LOG_NOP
#define NK_LOG_WRN(...) _NK_LOG_NOP
#define NK_LOG_INF(...) _NK_LOG_NOP
#define NK_LOG_DBG(...) _NK_LOG_NOP
#define NK_LOG_TRC(...) _NK_LOG_NOP

#endif // ENABLE_LOGGING

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_LOGGER
