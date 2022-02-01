#ifdef ENABLE_LOGGING

#include "nk/common/logger.hpp"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>

#include <unistd.h>

namespace {

static constexpr char const *c_env_var = "NK_LOG_LEVEL";

static constexpr char const *c_color_none = "\x1b[0m";
// static constexpr char const *c_color_gray = "\x1b[1;30m";
static constexpr char const *c_color_red = "\x1b[1;31m";
static constexpr char const *c_color_green = "\x1b[1;32m";
static constexpr char const *c_color_yellow = "\x1b[1;33m";
static constexpr char const *c_color_blue = "\x1b[1;34m";
static constexpr char const *c_color_magenta = "\x1b[1;35m";
// static constexpr char const *c_color_cyan = "\x1b[1;36m";
// static constexpr char const *c_color_white = "\x1b[1;37m";

constexpr char const *c_color_map[] = {
    nullptr,         // None
    c_color_red,     // Error
    c_color_yellow,  // Warning
    c_color_blue,    // Info
    c_color_green,   // Debug
    c_color_magenta, // Trace
};

constexpr char const *c_log_level_map[] = {
    nullptr,
    "[ERROR] ",
    "[WARNING] ",
    "[INFO] ",
    "[DEBUG] ",
    "[TRACE] ",
};

std::string const c_env_log_level_map[] = {
    "none",
    "error",
    "warning",
    "info",
    "debug",
    "trace",
};

struct LoggerState {
    std::chrono::steady_clock::time_point start_time;
    ELogLevel log_level;
    EColorMode color_mode;
    std::unique_ptr<std::ostream> out;
    std::mutex mutex;
};

LoggerState &instance() {
    static LoggerState instance;
    return instance;
}

ELogLevel parseEnvLogLevel(char const *env_log_level) {
    size_t i = 0;
    for (; i <= Log_Trace; i++) {
        if (env_log_level == c_env_log_level_map[i]) {
            return (ELogLevel)i;
        }
    }
    return Log_None;
}

} // namespace

void _logger_write(ELogLevel log_level, char const *scope, char const *fmt, ...) {
    auto &logger = instance();

    char const *env_log_level = std::getenv(c_env_var);
    if (!logger.out ||
        log_level > (env_log_level ? parseEnvLogLevel(env_log_level) : logger.log_level)) {
        return;
    }

    bool const to_color = logger.color_mode == Log_Color_Always ||
                          (logger.color_mode == Log_Color_Auto && isatty(fileno(stdout)));

    auto &out = *logger.out;

    std::lock_guard<std::mutex> guard{logger.mutex};

    if (to_color) {
        out << c_color_none << c_color_map[log_level];
    }
    out << c_log_level_map[log_level];

    auto const ts = std::chrono::duration_cast<std::chrono::duration<double>>(
                        std::chrono::steady_clock::now() - logger.start_time)
                        .count();
    out << ts << " " << scope << " :: ";

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (to_color) {
        out << c_color_none;
    }

    out << "\n";
}

void _logger_init(LoggerOptions opt) {
    auto &logger = instance();

    logger.start_time = std::chrono::steady_clock::now();

    logger.log_level = opt.log_level;
    logger.color_mode = opt.color_mode;

    logger.out.reset(new std::ostream{std::cerr.rdbuf()});
    *logger.out << std::fixed << std::setprecision(6);
}

#endif // ENABLE_LOGGING
