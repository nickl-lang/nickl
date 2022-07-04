#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
// #include "nkl/core/core.hpp"

static void printErrorUsage() {
    fprintf(stderr, "See `%s --help` for usage information\n", NKL_BINARY_NAME);
}

static void printUsage() {
    printf(
        "Usage: %s [options] file"
        "\nOptions:"
#ifdef ENABLE_LOGGING
        "\n    -c, --color {auto,always,never}           Choose when to color output"
        "\n    -l, --loglevel {none,info,debug,trace}    Select logging level"
#endif // ENABLE_LOGGING
        "\n    -h, --help                                Display this message and exit"
        "\n    -v, --version                             Show version information"
        "\n",
        NKL_BINARY_NAME);
}

static void printVersion() {
    printf(
        "%s v%i.%i.%i\n", NKL_BINARY_NAME, NKL_VERSION_MAJOR, NKL_VERSION_MINOR, NKL_VERSION_PATCH);
}

static bool eql(char const *lhs, char const *rhs) {
    return lhs && rhs && strcmp(lhs, rhs) == 0;
}

int main(int argc, char const *const *argv) {
    ProfilerWrapper prof;

    char const *in_file = nullptr;
    bool help = false;
    bool version = false;

#ifdef ENABLE_LOGGING
    LoggerOptions logger_options{};
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
                        logger_options.color_mode = Log_Color_Auto;
                    } else if (eql(argv[i], "always")) {
                        logger_options.color_mode = Log_Color_Always;
                    } else if (eql(argv[i], "never")) {
                        logger_options.color_mode = Log_Color_Never;
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
                        logger_options.log_level = Log_None;
                    } else if (eql(argv[i], "info")) {
                        logger_options.log_level = Log_Info;
                    } else if (eql(argv[i], "debug")) {
                        logger_options.log_level = Log_Debug;
                    } else if (eql(argv[i], "trace")) {
                        logger_options.log_level = Log_Trace;
                    } else {
                        fprintf(
                            stderr,
                            "error: invalid loglevel `%s`. Possible values are `none`, `info`, "
                            "`debug`, "
                            "`trace`\n",
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

    LOGGER_INIT(logger_options);

    // nkl::lang_init();
    // int ret_code = nkl::lang_runFile(cs2s(in_file));
    // nkl::lang_deinit();

    return 0; // ret_code;
}
