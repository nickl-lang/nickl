#ifndef NTK_LOG_H_
#define NTK_LOG_H_

#include <stdarg.h>

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NkLogLevel_None = 0,
    NkLogLevel_Fatal,
    NkLogLevel_Error,
    NkLogLevel_Warning,
    NkLogLevel_Info,
    NkLogLevel_Debug,
    NkLogLevel_Trace,
} NkLogLevel;

typedef enum {
    NkLogColorMode_Auto = 0,
    NkLogColorMode_Always,
    NkLogColorMode_Never,
} NkLogColorMode;

typedef struct {
    NkLogLevel log_level;
    NkLogColorMode color_mode;
} NkLogOptions;

bool nk_log_check(NkLogLevel log_level);
NK_PRINTF_LIKE(3) void nk_log_write(NkLogLevel log_level, char const *scope, char const *fmt, ...);

void nk_log_vwrite(NkLogLevel log_level, char const *scope, char const *fmt, va_list ap);

void nk_log_init(NkLogOptions opt);

#ifdef __cplusplus
}
#endif

#ifdef ENABLE_LOGGING

#define NK_LOG_INIT(...) nk_log_init(__VA_ARGS__)

#define NK_LOG_USE_SCOPE(NAME) static char const *_nk_log_scope = #NAME

#define NK_LOG_SEV(LEVEL, ...)                             \
    if (nk_log_check(LEVEL)) {                             \
        nk_log_write((LEVEL), _nk_log_scope, __VA_ARGS__); \
    } else                                                 \
        _NK_NOP

#define NK_LOGV_SEV(LEVEL, FMT, AP)                         \
    if (nk_log_check(LEVEL)) {                              \
        nk_log_vwrite((LEVEL), _nk_log_scope, (FMT), (AP)); \
    } else                                                  \
        _NK_NOP

#define NK_LOG_FAT(...) NK_LOG_SEV(NkLogLevel_Fatal, __VA_ARGS__)
#define NK_LOG_ERR(...) NK_LOG_SEV(NkLogLevel_Error, __VA_ARGS__)
#define NK_LOG_WRN(...) NK_LOG_SEV(NkLogLevel_Warning, __VA_ARGS__)
#define NK_LOG_INF(...) NK_LOG_SEV(NkLogLevel_Info, __VA_ARGS__)
#define NK_LOG_DBG(...) NK_LOG_SEV(NkLogLevel_Debug, __VA_ARGS__)
#define NK_LOG_TRC(...) NK_LOG_SEV(NkLogLevel_Trace, __VA_ARGS__)

#else // ENABLE_LOGGING

#define NK_LOG_INIT(...) _NK_NOP

#define NK_LOG_USE_SCOPE(...) _NK_NOP_TOPLEVEL

#define NK_LOG_SEV(...) _NK_NOP
#define NK_LOGV_SEV(...) _NK_NOP

#define NK_LOG_FAT(...) _NK_NOP
#define NK_LOG_ERR(...) _NK_NOP
#define NK_LOG_WRN(...) _NK_NOP
#define NK_LOG_INF(...) _NK_NOP
#define NK_LOG_DBG(...) _NK_NOP
#define NK_LOG_TRC(...) _NK_NOP

#endif // ENABLE_LOGGING

#endif // NTK_LOG_H_
