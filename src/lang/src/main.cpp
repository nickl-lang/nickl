#include "main.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "nkl/lang/compiler.h"
#include "ntk/logger.h"
#include "ntk/profiler.hpp"
#include "ntk/sys/app.hpp"
#include "ntk/utils.hpp"

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

bool eql(char const *lhs, char const *rhs) {
    return lhs && rhs && strcmp(lhs, rhs) == 0;
}

} // namespace

int nkl_main(int argc, char const *const *argv) {
#ifdef BUILD_WITH_EASY_PROFILER
    EASY_PROFILER_ENABLE;
    ::profiler::startListen(EASY_PROFILER_PORT);
#endif // BUILD_WITH_EASY_PROFILER

    char const *in_file = nullptr;
    bool help = false;
    bool version = false;

#ifdef ENABLE_LOGGING
    NkLoggerOptions logger_options{};
    logger_options.log_level = NkLog_Error;
#endif // ENABLE_LOGGING

    for (int i = 1; i < argc; i++) {
        char const *arg = argv[i];

        if (arg[0] == '-') {
            if (eql(arg, "-h") || eql(arg, "--help")) {
                help = true;
            } else if (eql(arg, "-v") || eql(arg, "--version")) {
                version = true;
            } else {
#ifdef ENABLE_LOGGING
                i++;
                if (eql(arg, "-c") || eql(arg, "--color")) {
                    if (!argv[i]) {
                        fprintf(stderr, "error: argument required\n");
                        printErrorUsage();
                        return 1;
                    }
                    if (eql(argv[i], "auto")) {
                        logger_options.color_mode = NkLog_Color_Auto;
                    } else if (eql(argv[i], "always")) {
                        logger_options.color_mode = NkLog_Color_Always;
                    } else if (eql(argv[i], "never")) {
                        logger_options.color_mode = NkLog_Color_Never;
                    } else {
                        fprintf(
                            stderr,
                            "error: invalid color mode `%s`. Possible values are `auto`, `always`, "
                            "`never`\n",
                            argv[i]);
                        printErrorUsage();
                        return 1;
                    }
                } else if (eql(arg, "-l") || eql(arg, "--loglevel")) {
                    if (!argv[i]) {
                        fprintf(stderr, "error: argument required\n");
                        printErrorUsage();
                        return 1;
                    }
                    if (eql(argv[i], "none")) {
                        logger_options.log_level = NkLog_None;
                    } else if (eql(argv[i], "error")) {
                        logger_options.log_level = NkLog_Error;
                    } else if (eql(argv[i], "warning")) {
                        logger_options.log_level = NkLog_Warning;
                    } else if (eql(argv[i], "info")) {
                        logger_options.log_level = NkLog_Info;
                    } else if (eql(argv[i], "debug")) {
                        logger_options.log_level = NkLog_Debug;
                    } else if (eql(argv[i], "trace")) {
                        logger_options.log_level = NkLog_Trace;
                    } else {
                        fprintf(
                            stderr,
                            "error: invalid loglevel `%s`. Possible values are `none`, `error`, "
                            "`warning`, `info`, `debug`, `trace`\n",
                            argv[i]);
                        printErrorUsage();
                        return 1;
                    }
                } else
#endif // ENABLE_LOGGING
                {
                    fprintf(stderr, "error: invalid argument `%s`\n", arg);
                    printErrorUsage();
                    return 1;
                }
            }
        } else if (!in_file) {
            in_file = arg;
        } else {
            fprintf(stderr, "error: extra argument `%s`\n", arg);
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

    if (!in_file) {
        fprintf(stderr, "error: no input file\n");
        printErrorUsage();
        return 1;
    }

    NK_LOGGER_INIT(logger_options);

    auto compiler = nkl_compiler_create();
    if (!nkl_compiler_configure(compiler, nk_mkstr(nk_appDir().string().c_str()))) {
        return 1;
    }
    defer {
        nkl_compiler_free(compiler);
    };
    if (!nkl_compiler_runFile(compiler, nk_mkstr(in_file))) {
        return 1;
    }

#ifdef BUILD_WITH_EASY_PROFILER
    puts("press any key to exit");
    getchar();
#endif // BUILD_WITH_EASY_PROFILER

    return 0;
}
