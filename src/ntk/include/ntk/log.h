#ifndef NTK_LOG_H_
#define NTK_LOG_H_

#include <stdarg.h>

#include "ntk/common.h"
#include "ntk/stream.h"

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

NK_EXPORT bool nk_log_check(NkLogLevel log_level);
NK_EXPORT NK_PRINTF_LIKE(3) void nk_log_write(NkLogLevel log_level, char const *scope, char const *fmt, ...);

NK_EXPORT void nk_log_vwrite(NkLogLevel log_level, char const *scope, char const *fmt, va_list ap);

NK_EXPORT void nk_log_streamOpen(NkLogLevel log_level, char const *scope);
NK_EXPORT void nk_log_streamClose(void);

// NOTE: Only safe to use between nk_log_streamOpen and nk_log_streamClose
NK_EXPORT NkStream nk_log_getStream();

NK_EXPORT void nk_log_setStream(NkStream out);

NK_EXPORT void nk_log_init(NkLogOptions opt);

#ifdef __cplusplus
}
#endif

#ifdef ENABLE_LOGGING

#define NK_LOG_INIT(...) nk_log_init(__VA_ARGS__)

#define NK_LOG_USE_SCOPE(NAME) static char const *_nk_log_scope = #NAME

#define NK_LOG(LEVEL, ...) (nk_log_check(LEVEL) && (nk_log_write((LEVEL), _nk_log_scope, __VA_ARGS__), 1))
#define NK_LOGV(LEVEL, FMT, AP) (nk_log_check(LEVEL) && (nk_log_vwrite((LEVEL), _nk_log_scope, (FMT), (AP)), 1))

#define NK_LOG_FAT(...) NK_LOG(NkLogLevel_Fatal, __VA_ARGS__)
#define NK_LOG_ERR(...) NK_LOG(NkLogLevel_Error, __VA_ARGS__)
#define NK_LOG_WRN(...) NK_LOG(NkLogLevel_Warning, __VA_ARGS__)
#define NK_LOG_INF(...) NK_LOG(NkLogLevel_Info, __VA_ARGS__)
#define NK_LOG_DBG(...) NK_LOG(NkLogLevel_Debug, __VA_ARGS__)
#define NK_LOG_TRC(...) NK_LOG(NkLogLevel_Trace, __VA_ARGS__)

#define NK_LOG_STREAM(LEVEL) \
    NK_DEFER_LOOP_OPT(nk_log_check(LEVEL), nk_log_streamOpen(LEVEL, _nk_log_scope), nk_log_streamClose())

#define NK_LOG_STREAM_FAT NK_LOG_STREAM(NkLogLevel_Fatal)
#define NK_LOG_STREAM_ERR NK_LOG_STREAM(NkLogLevel_Error)
#define NK_LOG_STREAM_WRN NK_LOG_STREAM(NkLogLevel_Warning)
#define NK_LOG_STREAM_INF NK_LOG_STREAM(NkLogLevel_Info)
#define NK_LOG_STREAM_DBG NK_LOG_STREAM(NkLogLevel_Debug)
#define NK_LOG_STREAM_TRC NK_LOG_STREAM(NkLogLevel_Trace)

#else // ENABLE_LOGGING

#define NK_LOG_INIT(...) _NK_NOP

#define NK_LOG_USE_SCOPE(...) _NK_NOP_TOPLEVEL

#define NK_LOG(...) _NK_NOP
#define NK_LOGV(...) _NK_NOP

#define NK_LOG_FAT(...) NK_LOG()
#define NK_LOG_ERR(...) NK_LOG()
#define NK_LOG_WRN(...) NK_LOG()
#define NK_LOG_INF(...) NK_LOG()
#define NK_LOG_DBG(...) NK_LOG()
#define NK_LOG_TRC(...) NK_LOG()

#define NK_LOG_STREAM(...) while (0)

#define NK_LOG_STREAM_FAT NK_LOG_STREAM()
#define NK_LOG_STREAM_ERR NK_LOG_STREAM()
#define NK_LOG_STREAM_WRN NK_LOG_STREAM()
#define NK_LOG_STREAM_INF NK_LOG_STREAM()
#define NK_LOG_STREAM_DBG NK_LOG_STREAM()
#define NK_LOG_STREAM_TRC NK_LOG_STREAM()

#endif // ENABLE_LOGGING

#endif // NTK_LOG_H_
