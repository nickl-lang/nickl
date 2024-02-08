#include "nkb/common.h"
#include "nkb/ir.h"
#include "nkl/common/diagnostics.h"
#include "nkl/common/token.h"
#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/cli.h"
#include "ntk/file.h"
#include "ntk/logger.h"
#include "ntk/profiler.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/sys/file.h"
#include "ntk/sys/path.h"
#include "ntk/utils.h"
#include "stc.h"

namespace {

void printErrorUsage() {
    nk_printf(nk_file_getStream(nk_stderr()), "See `%s --help` for usage information\n", NK_BINARY_NAME);
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
    nks in_file{};

    bool help = false;
    bool version = false;

#ifdef ENABLE_LOGGING
    NkLoggerOptions logger_opts{};
    logger_opts.log_level = NkLog_Error;
#endif // ENABLE_LOGGING

    for (argv++; *argv;) {
        nks key{};
        nks val{};
        NK_CLI_ARG_INIT(&argv, &key, &val);

#define GET_VALUE                                                                             \
    do {                                                                                      \
        NK_CLI_ARG_GET_VALUE;                                                                 \
        if (!val.size) {                                                                      \
            nkl_diag_printError("argument `" nks_Fmt "` requires a parameter", nks_Arg(key)); \
            printErrorUsage();                                                                \
            return 1;                                                                         \
        }                                                                                     \
    } while (0)

#define NO_VALUE                                                                                   \
    do {                                                                                           \
        if (val.size) {                                                                            \
            nkl_diag_printError("argument `" nks_Fmt "` doesn't accept parameters", nks_Arg(key)); \
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
                        "invalid color mode `" nks_Fmt "`. Possible values are `auto`, `always`, `never`",
                        nks_Arg(val));
                    printErrorUsage();
                    return 1;
                }
#ifdef ENABLE_LOGGING
                if (val == "auto") {
                    logger_opts.color_mode = NkLog_Color_Auto;
                } else if (val == "always") {
                    logger_opts.color_mode = NkLog_Color_Always;
                } else if (val == "never") {
                    logger_opts.color_mode = NkLog_Color_Never;
                }
            } else if (key == "-t" || key == "--loglevel") {
                GET_VALUE;
                if (val == "none") {
                    logger_opts.log_level = NkLog_None;
                } else if (val == "error") {
                    logger_opts.log_level = NkLog_Error;
                } else if (val == "warning") {
                    logger_opts.log_level = NkLog_Warning;
                } else if (val == "info") {
                    logger_opts.log_level = NkLog_Info;
                } else if (val == "debug") {
                    logger_opts.log_level = NkLog_Debug;
                } else if (val == "trace") {
                    logger_opts.log_level = NkLog_Trace;
                } else {
                    nkl_diag_printError(
                        "invalid loglevel `" nks_Fmt
                        "`. Possible values are `none`, `error`, `warning`, `info`, `debug`, `trace`",
                        nks_Arg(val));
                    printErrorUsage();
                    return 1;
                }
#endif // ENABLE_LOGGING
            } else {
                nkl_diag_printError("invalid argument `" nks_Fmt "`", nks_Arg(key));
                printErrorUsage();
                return 1;
            }
        } else if (!in_file.size) {
            in_file = val;
        } else {
            nkl_diag_printError("extra argument `" nks_Fmt "`", nks_Arg(val));
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

    NK_LOGGER_INIT(logger_opts);

    int code = nkst_compile(in_file);

    return code;
}
