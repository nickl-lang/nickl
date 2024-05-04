#include "main.h"

#include <stdio.h>

#include "nkl/lang/compiler.h"
#include "ntk/cli.h"
#include "ntk/log.h"
#include "ntk/os/path.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
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

int nkl_main(int /*argc*/, char const *const *argv) {
    NK_PROF_START(NKL_BINARY_NAME ".spall");
    NK_PROF_THREAD_ENTER(0, 32 * 1024 * 1024);
    defer {
        NK_PROF_THREAD_LEAVE();
        NK_PROF_FINISH();
    };

    NK_PROF_FUNC();

    NkString in_file{};
    bool help = false;
    bool version = false;

#ifdef ENABLE_LOGGING
    NkLogOptions log_options{};
    log_options.log_level = NkLogLevel_Error;
#endif // ENABLE_LOGGING

    for (argv++; *argv;) {
        NkString key{};
        NkString val{};
        NK_CLI_ARG_INIT(&argv, &key, &val);

#define GET_VALUE                                                                                  \
    do {                                                                                           \
        NK_CLI_ARG_GET_VALUE;                                                                      \
        if (!val.size) {                                                                           \
            fprintf(stderr, "error: argument `" NKS_FMT "` requires a parameter\n", NKS_ARG(key)); \
            printErrorUsage();                                                                     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

#define NO_VALUE                                                                                        \
    do {                                                                                                \
        if (val.size) {                                                                                 \
            fprintf(stderr, "error: argument `" NKS_FMT "` doesn't accept parameters\n", NKS_ARG(key)); \
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
                    log_options.color_mode = NkLogColorMode_Auto;
                } else if (val == "always") {
                    log_options.color_mode = NkLogColorMode_Always;
                } else if (val == "never") {
                    log_options.color_mode = NkLogColorMode_Never;
                } else {
                    fprintf(
                        stderr,
                        "error: invalid color mode `" NKS_FMT "`. Possible values are `auto`, `always`, `never`\n",
                        NKS_ARG(val));
                    printErrorUsage();
                    return 1;
                }
            } else if (key == "-l" || key == "--loglevel") {
                GET_VALUE;
                if (val == "none") {
                    log_options.log_level = NkLogLevel_None;
                } else if (val == "error") {
                    log_options.log_level = NkLogLevel_Error;
                } else if (val == "warning") {
                    log_options.log_level = NkLogLevel_Warning;
                } else if (val == "info") {
                    log_options.log_level = NkLogLevel_Info;
                } else if (val == "debug") {
                    log_options.log_level = NkLogLevel_Debug;
                } else if (val == "trace") {
                    log_options.log_level = NkLogLevel_Trace;
                } else {
                    fprintf(
                        stderr,
                        "error: invalid loglevel `" NKS_FMT
                        "`. Possible values are `none`, `error`, `warning`, `info`, `debug`, `trace`\n",
                        NKS_ARG(val));
                    printErrorUsage();
                    return 1;
                }
#endif // ENABLE_LOGGING
            } else {
                fprintf(stderr, "error: invalid argument `" NKS_FMT "`\n", NKS_ARG(key));
                printErrorUsage();
                return 1;
            }
        } else if (!in_file.size) {
            in_file = val;
        } else {
            fprintf(stderr, "error: extra argument `" NKS_FMT "`\n", NKS_ARG(val));
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

    NK_LOG_INIT(log_options);

    nk_atom_init();
    defer {
        nk_atom_deinit();
    };

    char path_buf[NK_MAX_PATH];
    int path_len = nk_getBinaryPath(path_buf, sizeof(path_buf));
    if (path_len < 0) {
        fprintf(stderr, "error: failed to get the compiler binary path\n");
        return 1;
    }

    auto compiler = nkl_compiler_create();
    if (!nkl_compiler_configure(compiler, {path_buf, (usize)path_len})) {
        return 1;
    }
    defer {
        nkl_compiler_free(compiler);
    };
    if (!nkl_compiler_runFile(compiler, in_file)) {
        return 1;
    }

    return 0;
}
