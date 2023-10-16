#include "main.h"

#include <stdio.h>

#include "nkl/lang/compiler.h"
#include "ntk/cli.h"
#include "ntk/logger.h"
#include "ntk/profiler.hpp"
#include "ntk/string.h"
#include "ntk/sys/path.h"
#include "ntk/utils.h"

namespace {

void printErrorUsage() {
    fprintf(stderr, "See `%s --help` for usage information\n", NKL_BINARY_NAME);
}

void printUsage() {
    printf("Usage: " NKL_BINARY_NAME
           " [options] file"
           "\nOptions:"
#ifdef ENABLE_LOGGING
           "\n    -c, --color {auto,always,never}                      Choose when to color output"
           "\n    -l, --loglevel {none,error,warning,info,debug,trace} Select logging level"
#endif // ENABLE_LOGGING
           "\n    -h, --help                                           Display this message and exit"
           "\n    -v, --version                                        Show version information"
           "\n");
}

void printVersion() {
    printf(NKL_BINARY_NAME " " NKL_BUILD_VERSION " " NKL_BUILD_TIME "\n");
}

} // namespace

int nkl_main(int argc, char const *const *argv) {
#ifdef BUILD_WITH_EASY_PROFILER
    EASY_PROFILER_ENABLE;
    ::profiler::startListen(EASY_PROFILER_PORT);
#endif // BUILD_WITH_EASY_PROFILER

    nks in_file{};
    bool help = false;
    bool version = false;

#ifdef ENABLE_LOGGING
    NkLoggerOptions logger_options{};
    logger_options.log_level = NkLog_Error;
#endif // ENABLE_LOGGING

    for (argv++; *argv;) {
        nks key{};
        nks val{};
        NK_CLI_ARG_INIT(&argv, &key, &val);

#define GET_VALUE                                                                                  \
    do {                                                                                           \
        NK_CLI_ARG_GET_VALUE;                                                                      \
        if (!val.size) {                                                                           \
            fprintf(stderr, "error: argument `" nks_Fmt "` requires a parameter\n", nks_Arg(key)); \
            printErrorUsage();                                                                     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

#define NO_VALUE                                                                                        \
    do {                                                                                                \
        if (val.size) {                                                                                 \
            fprintf(stderr, "error: argument `" nks_Fmt "` doesn't accept parameters\n", nks_Arg(key)); \
            printErrorUsage();                                                                          \
            return 1;                                                                                   \
        }                                                                                               \
    } while (0)

        if (key.size) {
            if (key == "-h" || key == "--help") {
                NO_VALUE;
                help = true;
            } else if (key == "-v" || key == "--version") {
                NO_VALUE;
                version = true;
#ifdef ENABLE_LOGGING
            } else if (key == "-c" || key == "--color") {
                GET_VALUE;
                if (val == "auto") {
                    logger_options.color_mode = NkLog_Color_Auto;
                } else if (val == "always") {
                    logger_options.color_mode = NkLog_Color_Always;
                } else if (val == "never") {
                    logger_options.color_mode = NkLog_Color_Never;
                } else {
                    fprintf(
                        stderr,
                        "error: invalid color mode `" nks_Fmt "`. Possible values are `auto`, `always`, `never`\n",
                        nks_Arg(val));
                    printErrorUsage();
                    return 1;
                }
            } else if (key == "-l" || key == "--loglevel") {
                GET_VALUE;
                if (val == "none") {
                    logger_options.log_level = NkLog_None;
                } else if (val == "error") {
                    logger_options.log_level = NkLog_Error;
                } else if (val == "warning") {
                    logger_options.log_level = NkLog_Warning;
                } else if (val == "info") {
                    logger_options.log_level = NkLog_Info;
                } else if (val == "debug") {
                    logger_options.log_level = NkLog_Debug;
                } else if (val == "trace") {
                    logger_options.log_level = NkLog_Trace;
                } else {
                    fprintf(
                        stderr,
                        "error: invalid loglevel `" nks_Fmt
                        "`. Possible values are `none`, `error`, `warning`, `info`, `debug`, `trace`\n",
                        nks_Arg(val));
                    printErrorUsage();
                    return 1;
                }
#endif // ENABLE_LOGGING
            } else {
                fprintf(stderr, "error: invalid argument `" nks_Fmt "`\n", nks_Arg(key));
                printErrorUsage();
                return 1;
            }
        } else if (!in_file.size) {
            in_file = val;
        } else {
            fprintf(stderr, "error: extra argument `" nks_Fmt "`\n", nks_Arg(val));
            printErrorUsage();
            return 1;
        }
    }

    if (help) {
        printUsage();
        return 0;
    }

    if (version) {
        printVersion();
        return 0;
    }

    if (!in_file.size) {
        fprintf(stderr, "error: no input file\n");
        printErrorUsage();
        return 1;
    }

    NK_LOGGER_INIT(logger_options);

    char path_buf[NK_MAX_PATH];
    int path_len = nk_getBinaryPath(path_buf, sizeof(path_buf));
    if (path_len < 0) {
        fprintf(stderr, "error: failed to get the compiler binary path\n");
        return 1;
    }

    auto compiler = nkl_compiler_create();
    if (!nkl_compiler_configure(compiler, {path_buf, (size_t)path_len})) {
        return 1;
    }
    defer {
        nkl_compiler_free(compiler);
    };
    if (!nkl_compiler_runFile(compiler, in_file)) {
        return 1;
    }

#ifdef BUILD_WITH_EASY_PROFILER
    puts("press any key to exit");
    getchar();
#endif // BUILD_WITH_EASY_PROFILER

    return 0;
}
