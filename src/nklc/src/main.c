#include "nkl/common/diagnostics.h"
#include "ntk/cli.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/utils.h"

static void printErrorUsage() {
    nk_stream_printf(nk_file_getStream(nk_stderr()), "See `%s --help` for usage information\n", NK_BINARY_NAME);
}

static void printUsage() {
    printf(
        "Usage: " NK_BINARY_NAME
        " [options] file"
        "\nOptions:"
        "\n    -c, --color {auto,always,never}          Choose when to color output"
        "\n    -h, --help                               Display this message and exit"
        "\n    -v, --version                            Show version information"
#ifdef ENABLE_LOGGING
        "\nDeveloper options:"
        "\n    -t, --loglevel {none,error,warning,info,debug,trace}   Select logging level"
#endif // ENABLE_LOGGING
        "\n");
}

static void printVersion() {
    printf(NK_BINARY_NAME " " NK_BUILD_VERSION " " NK_BUILD_TIME "\n");
}

int main(int NK_UNUSED argc, char const *const *argv) {
    NK_DEFER_LOOP(NK_PROF_START(NK_BINARY_NAME ".spall"), NK_PROF_FINISH())
    NK_DEFER_LOOP(NK_PROF_THREAD_ENTER(0, 32 * 1024 * 1024), NK_PROF_THREAD_LEAVE())
    NK_DEFER_LOOP(NK_PROF_FUNC_BEGIN(), NK_PROF_FUNC_END()) {
        NkString in_file = {0};

        bool help = false;
        bool version = false;

#ifdef ENABLE_LOGGING
        NkLogOptions log_opts = {0};
        log_opts.log_level = NkLogLevel_Warning;
#endif // ENABLE_LOGGING

        for (argv++; *argv;) {
            NkString key = {0};
            NkString val = {0};
            NK_CLI_ARG_INIT(&argv, &key, &val);

#define GET_VALUE                                                                             \
    do {                                                                                      \
        NK_CLI_ARG_GET_VALUE;                                                                 \
        if (!val.size) {                                                                      \
            nkl_diag_printError("argument `" NKS_FMT "` requires a parameter", NKS_ARG(key)); \
            printErrorUsage();                                                                \
            return 1;                                                                         \
        }                                                                                     \
    } while (0)

#define NO_VALUE                                                                                   \
    do {                                                                                           \
        if (val.size) {                                                                            \
            nkl_diag_printError("argument `" NKS_FMT "` doesn't accept parameters", NKS_ARG(key)); \
            printErrorUsage();                                                                     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

            if (key.size) {
                if (nks_equal(key, nk_cs2s("-h")) || nks_equal(key, nk_cs2s("--help"))) {
                    NO_VALUE;
                    help = true;
                } else if (nks_equal(key, nk_cs2s("-v")) || nks_equal(key, nk_cs2s("--version"))) {
                    NO_VALUE;
                    version = true;
                } else if (nks_equal(key, nk_cs2s("-c")) || nks_equal(key, nk_cs2s("--color"))) {
                    GET_VALUE;
                    if (nks_equal(val, nk_cs2s("auto"))) {
                        nkl_diag_init(NklColorPolicy_Auto);
                    } else if (nks_equal(val, nk_cs2s("always"))) {
                        nkl_diag_init(NklColorPolicy_Always);
                    } else if (nks_equal(val, nk_cs2s("never"))) {
                        nkl_diag_init(NklColorPolicy_Never);
                    } else {
                        nkl_diag_printError(
                            "invalid color mode `" NKS_FMT "`. Possible values are `auto`, `always`, `never`",
                            NKS_ARG(val));
                        printErrorUsage();
                        return 1;
                    }
#ifdef ENABLE_LOGGING
                    if (nks_equal(val, nk_cs2s("auto"))) {
                        log_opts.color_mode = NkLogColorMode_Auto;
                    } else if (nks_equal(val, nk_cs2s("always"))) {
                        log_opts.color_mode = NkLogColorMode_Always;
                    } else if (nks_equal(val, nk_cs2s("never"))) {
                        log_opts.color_mode = NkLogColorMode_Never;
                    }
                } else if (nks_equal(key, nk_cs2s("-t")) || nks_equal(key, nk_cs2s("--loglevel"))) {
                    GET_VALUE;
                    if (nks_equal(val, nk_cs2s("none"))) {
                        log_opts.log_level = NkLogLevel_None;
                    } else if (nks_equal(val, nk_cs2s("error"))) {
                        log_opts.log_level = NkLogLevel_Error;
                    } else if (nks_equal(val, nk_cs2s("warning"))) {
                        log_opts.log_level = NkLogLevel_Warning;
                    } else if (nks_equal(val, nk_cs2s("info"))) {
                        log_opts.log_level = NkLogLevel_Info;
                    } else if (nks_equal(val, nk_cs2s("debug"))) {
                        log_opts.log_level = NkLogLevel_Debug;
                    } else if (nks_equal(val, nk_cs2s("trace"))) {
                        log_opts.log_level = NkLogLevel_Trace;
                    } else {
                        nkl_diag_printError(
                            "invalid loglevel `" NKS_FMT
                            "`. Possible values are `none`, `error`, `warning`, `info`, `debug`, `trace`",
                            NKS_ARG(val));
                        printErrorUsage();
                        return 1;
                    }
#endif // ENABLE_LOGGING
                } else {
                    nkl_diag_printError("invalid argument `" NKS_FMT "`", NKS_ARG(key));
                    printErrorUsage();
                    return 1;
                }
            } else if (!in_file.size) {
                in_file = val;
            } else {
                nkl_diag_printError("extra argument `" NKS_FMT "`", NKS_ARG(val));
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
            nkl_diag_printError("no input file");
            printErrorUsage();
            return 1;
        }

        NK_LOG_INIT(log_opts);

        int code = 0;

        return code;
    }
}
