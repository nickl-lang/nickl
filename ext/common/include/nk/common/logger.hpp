#ifndef HEADER_GUARD_NK_COMMON_LOGGER
#define HEADER_GUARD_NK_COMMON_LOGGER

#ifdef ENABLE_LOGGING

enum ELogLevel {
    Log_None = 0,
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

void _logger_write(ELogLevel log_level, char const *scope, char const *fmt, ...);

void _logger_init(LoggerOptions opt);

#define LOGGER_INIT(...) _logger_init(__VA_ARGS__)

#define LOG_USE_SCOPE(name) static char const *__logger_scope = #name;

#define LOG_INF(...) _logger_write(Log_Info, __logger_scope, __VA_ARGS__);
#define LOG_DBG(...) _logger_write(Log_Debug, __logger_scope, __VA_ARGS__);
#define LOG_TRC(...) _logger_write(Log_Trace, __logger_scope, __VA_ARGS__);

#else // ENABLE_LOGGING

#define DEFAULT_INIT_LOGGER

#define LOGGER_INIT(...)

#define LOG_USE_SCOPE(...)

#define LOG_INF(...)
#define LOG_DBG(...)
#define LOG_TRC(...)

#endif // ENABLE_LOGGING

#endif // HEADER_GUARD_NK_COMMON_LOGGER
