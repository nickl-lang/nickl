#ifndef HEADER_GUARD_NK_COMMON_LOGGER
#define HEADER_GUARD_NK_COMMON_LOGGER

#ifdef ENABLE_LOGGING

enum ELogLevel {
    Log_None = 0,
    Log_Error,
    Log_Warning,
    Log_Info,
    Log_Debug,
    Log_Trace,
};

enum EColorMode {
    Log_Color_Auto = 0,
    Log_Color_Always,
    Log_Color_Never,
};

struct LoggerOptions {
    ELogLevel log_level;
    EColorMode color_mode;
};

bool _logger_check(ELogLevel log_level);
void _logger_write(ELogLevel log_level, char const *scope, char const *fmt, ...);

void _logger_init(LoggerOptions opt);

#define _LOG_CHK(LEVEL, ...)                               \
    if (_logger_check(LEVEL)) {                            \
        _logger_write(LEVEL, __logger_scope, __VA_ARGS__); \
    }

#define LOGGER_INIT(...) _logger_init(__VA_ARGS__)

#define LOG_USE_SCOPE(NAME) static char const *__logger_scope = #NAME;

#define LOG_ERR(...) _LOG_CHK(Log_Error, __VA_ARGS__);
#define LOG_WRN(...) _LOG_CHK(Log_Warning, __VA_ARGS__);
#define LOG_INF(...) _LOG_CHK(Log_Info, __VA_ARGS__);
#define LOG_DBG(...) _LOG_CHK(Log_Debug, __VA_ARGS__);
#define LOG_TRC(...) _LOG_CHK(Log_Trace, __VA_ARGS__);

#else // ENABLE_LOGGING

#define DEFAULT_INIT_LOGGER

#define LOGGER_INIT(...)

#define LOG_USE_SCOPE(...)

#define LOG_ERR(...)
#define LOG_WRN(...)
#define LOG_INF(...)
#define LOG_DBG(...)
#define LOG_TRC(...)

#endif // ENABLE_LOGGING

#endif // HEADER_GUARD_NK_COMMON_LOGGER
