#ifndef HEADER_GUARD_NK_COMMON_LOGGER
#define HEADER_GUARD_NK_COMMON_LOGGER

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_LOGGING

typedef enum {
    NkLog_None = 0,
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
void _nk_loggerWrite(NkLogLevel log_level, char const *scope, char const *fmt, ...);

void _nk_loggerInit(NkLoggerOptions opt);

#define _NK_LOG_CHK(LEVEL, ...)                                 \
    if (_nk_loggerCheck(LEVEL)) {                               \
        _nk_loggerWrite(LEVEL, __nk_logger_scope, __VA_ARGS__); \
    } else                                                      \
        (void)0

#define NK_LOGGER_INIT(...) _nk_loggerInit(__VA_ARGS__)

#define NK_LOG_USE_SCOPE(NAME) static char const *__nk_logger_scope = #NAME

#define NK_LOG_ERR(...) _NK_LOG_CHK(NkLog_Error, __VA_ARGS__)
#define NK_LOG_WRN(...) _NK_LOG_CHK(NkLog_Warning, __VA_ARGS__)
#define NK_LOG_INF(...) _NK_LOG_CHK(NkLog_Info, __VA_ARGS__)
#define NK_LOG_DBG(...) _NK_LOG_CHK(NkLog_Debug, __VA_ARGS__)
#define NK_LOG_TRC(...) _NK_LOG_CHK(NkLog_Trace, __VA_ARGS__)

#else // ENABLE_LOGGING

#define NK_LOGGER_INIT(...)

#define NK_LOG_USE_SCOPE(...)

#define NK_LOG_ERR(...)
#define NK_LOG_WRN(...)
#define NK_LOG_INF(...)
#define NK_LOG_DBG(...)
#define NK_LOG_TRC(...)

#endif // ENABLE_LOGGING

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_LOGGER
