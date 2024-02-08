#ifndef HEADER_GUARD_NTK_LOGGER
#define HEADER_GUARD_NTK_LOGGER

#include <stdarg.h>
#include <stdbool.h>

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

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

bool nk_loggerCheck(NkLogLevel log_level);
NK_PRINTF_LIKE(3, 4) void nk_loggerWrite(NkLogLevel log_level, char const *scope, char const *fmt, ...);

void nk_vloggerWrite(NkLogLevel log_level, char const *scope, char const *fmt, va_list ap);

void nk_loggerInit(NkLoggerOptions opt);

#ifdef __cplusplus
}
#endif

#ifdef ENABLE_LOGGING

#define NK_LOGGER_INIT(...) nk_loggerInit(__VA_ARGS__)

#define NK_LOG_USE_SCOPE(NAME) static char const *__nk_logger_scope = #NAME

#define NK_LOG_SEV(LEVEL, ...)                                   \
    if (nk_loggerCheck(LEVEL)) {                                 \
        nk_loggerWrite((LEVEL), __nk_logger_scope, __VA_ARGS__); \
    } else                                                       \
        (void)0

#define NK_VLOG_SEV(LEVEL, FMT, AP)                               \
    if (nk_loggerCheck(LEVEL)) {                                  \
        nk_vloggerWrite((LEVEL), __nk_logger_scope, (FMT), (AP)); \
    } else                                                        \
        (void)0

#define NK_LOG_FAT(...) NK_LOG_SEV(NkLog_Fatal, __VA_ARGS__)
#define NK_LOG_ERR(...) NK_LOG_SEV(NkLog_Error, __VA_ARGS__)
#define NK_LOG_WRN(...) NK_LOG_SEV(NkLog_Warning, __VA_ARGS__)
#define NK_LOG_INF(...) NK_LOG_SEV(NkLog_Info, __VA_ARGS__)
#define NK_LOG_DBG(...) NK_LOG_SEV(NkLog_Debug, __VA_ARGS__)
#define NK_LOG_TRC(...) NK_LOG_SEV(NkLog_Trace, __VA_ARGS__)

#else // ENABLE_LOGGING

#define NK_LOGGER_INIT(...) _NK_NOP

#define NK_LOG_USE_SCOPE(...) _NK_NOP_TOPLEVEL

#define NK_LOG_SEV(...) _NK_NOP
#define NK_VLOG_SEV(...) _NK_NOP

#define NK_LOG_FAT(...) _NK_NOP
#define NK_LOG_ERR(...) _NK_NOP
#define NK_LOG_WRN(...) _NK_NOP
#define NK_LOG_INF(...) _NK_NOP
#define NK_LOG_DBG(...) _NK_NOP
#define NK_LOG_TRC(...) _NK_NOP

#endif // ENABLE_LOGGING

#endif // HEADER_GUARD_NTK_LOGGER
