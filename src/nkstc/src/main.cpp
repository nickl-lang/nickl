#include "nkl/common/diagnostics.h"
#include "ntk/cli.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/os/file.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "stc.h"

namespace {

void printErrorUsage() {
    nk_stream_printf(nk_file_getStream(nk_stderr()), "See `%s --help` for usage information\n", NK_BINARY_NAME);
}

void printUsage() {
    printf("Usage: " NK_BINARY_NAME
           " [options] file"
           "\nOptions:"
           "\n    -h, --help                        Display this message and exit"
           "\n    -v, --version                     Show version information"
           "\n    -c, --color {auto,always,never}   Choose when to color output"
#ifdef ENABLE_LOGGING
           "\nDeveloper options:"
           "\n    -t, --loglevel {none,error,warning,info,debug,trace}   Select logging level"
#endif // ENABLE_LOGGING
           "\n");
}

void printVersion() {
    printf(NK_BINARY_NAME " " NK_BUILD_VERSION " " NK_BUILD_TIME "\n");
}

} // namespace

int main(int /*argc*/, char const *const *argv) {
    NkString in_file{};

    bool help = false;
    bool version = false;

#ifdef ENABLE_LOGGING
    NkLogOptions log_opts{};
    log_opts.log_level = NkLogLevel_Error;
#endif // ENABLE_LOGGING

    for (argv++; *argv;) {
        NkString key{};
        NkString val{};
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
            if (key == "-h" || key == "--help") {
                NO_VALUE;
                help = true;
            } else if (key == "-v" || key == "--version") {
                NO_VALUE;
                version = true;
            } else if (key == "-c" || key == "--color") {
                GET_VALUE;
                if (val == "auto") {
                    nkl_diag_init(NklColor_Auto);
                } else if (val == "always") {
                    nkl_diag_init(NklColor_Always);
                } else if (val == "never") {
                    nkl_diag_init(NklColor_Never);
                } else {
                    nkl_diag_printError(
                        "invalid color mode `" NKS_FMT "`. Possible values are `auto`, `always`, `never`",
                        NKS_ARG(val));
                    printErrorUsage();
                    return 1;
                }
#ifdef ENABLE_LOGGING
                if (val == "auto") {
                    log_opts.color_mode = NkLogColorMode_Auto;
                } else if (val == "always") {
                    log_opts.color_mode = NkLogColorMode_Always;
                } else if (val == "never") {
                    log_opts.color_mode = NkLogColorMode_Never;
                }
            } else if (key == "-t" || key == "--loglevel") {
                GET_VALUE;
                if (val == "none") {
                    log_opts.log_level = NkLogLevel_None;
                } else if (val == "error") {
                    log_opts.log_level = NkLogLevel_Error;
                } else if (val == "warning") {
                    log_opts.log_level = NkLogLevel_Warning;
                } else if (val == "info") {
                    log_opts.log_level = NkLogLevel_Info;
                } else if (val == "debug") {
                    log_opts.log_level = NkLogLevel_Debug;
                } else if (val == "trace") {
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

    int code = nkst_compile(in_file);

    return code;
}
