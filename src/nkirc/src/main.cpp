#include "config.hpp"
#include "diagnostics.h"
#include "irc.hpp"
#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/hash_map.hpp"
#include "ntk/logger.h"
#include "ntk/profiler.hpp"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/sys/path.h"
#include "ntk/utils.h"

namespace {

void printErrorUsage() {
    fprintf(stderr, "See `%s --help` for usage information\n", NK_BINARY_NAME);
}

void printUsage() {
    printf("Usage: " NK_BINARY_NAME
           " [options] file"
           "\nOptions:"
           "\n    -o, --output <file>                      Output file path"
           "\n    -k, --kind {run,exe,shared,static,obj}   Output file kind"
           "\n    -l, --link <lib>                         Link the library <lib>"
           "\n    -g                                       Add debug information"
           "\n    -c, --color {auto,always,never}          Choose when to color output"
           "\n    -h, --help                               Display this message and exit"
           "\n    -v, --version                            Show version information"
#ifdef ENABLE_LOGGING
           "\nDeveloper options:"
           "\n    -L, --loglevel {none,error,warning,info,debug,trace}   Select logging level"
#endif // ENABLE_LOGGING
           "\n");
}

void printVersion() {
    printf(NK_BINARY_NAME " " NK_BUILD_VERSION " " NK_BUILD_TIME "\n");
}

NK_LOG_USE_SCOPE(main);

} // namespace

int main(int, char const *const *argv) {
#ifdef BUILD_WITH_EASY_PROFILER
    EASY_PROFILER_ENABLE;
    ::profiler::startListen(EASY_PROFILER_PORT);
#endif // BUILD_WITH_EASY_PROFILER

    nks in_file{};
    nks out_file{};
    bool run = false;
    NkbOutputKind output_kind = NkbOutput_Executable;

    NkStringBuilder args{};
    defer {
        nksb_free(&args);
    };

    bool help = false;
    bool version = false;

#ifdef ENABLE_LOGGING
    NkLoggerOptions logger_opts{};
    logger_opts.log_level = NkLog_Error;
#endif // ENABLE_LOGGING

    for (auto parg = argv + 1; *parg;) {
        nks arg = nk_cs2s(*parg++);

        nks value{};
        nks key{};

        if (nks_starts_with(arg, nk_cs2s("--"))) {
            value = arg;
            key = nks_chop_by_delim(&value, '=');
        } else if (nks_first(arg) == '-') {
            key = {arg.data, minu(arg.size, 2)};
            if (arg.size > 2) {
                value = {arg.data + 2, arg.size - 2};
                if (nks_first(value) == '=') {
                    value.data += 1;
                    value.size -= 1;
                }
            }
        } else {
            value = arg;
        }

#define GET_VALUE                                                                                   \
    do {                                                                                            \
        if (!value.size) {                                                                          \
            if (!*parg || (*parg)[0] == '-') {                                                      \
                nkirc_diag_printError("argument `" nks_Fmt "` requires a parameter", nks_Arg(key)); \
                printErrorUsage();                                                                  \
                return 1;                                                                           \
            }                                                                                       \
            value = nk_cs2s(*parg++);                                                               \
        }                                                                                           \
    } while (0)

#define NO_VALUE                                                                                     \
    do {                                                                                             \
        if (value.size) {                                                                            \
            nkirc_diag_printError("argument `" nks_Fmt "` doesn't accept parameters", nks_Arg(key)); \
            printErrorUsage();                                                                       \
            return 1;                                                                                \
        }                                                                                            \
    } while (0)

        if (!key.size) {
            if (!in_file.data) {
                in_file = value;
            } else {
                nkirc_diag_printError("extra argument `" nks_Fmt "`", nks_Arg(value));
                printErrorUsage();
                return 1;
            }
        } else {
            if (key == "-o" || key == "--output") {
                GET_VALUE;
                out_file = value;
            } else if (key == "-k" || key == "--kind") {
                GET_VALUE;
                if (value == "run") {
                    run = true;
                } else if (value == "exe") {
                    output_kind = NkbOutput_Executable;
                } else if (value == "shared") {
                    output_kind = NkbOutput_Shared;
                } else if (value == "static") {
                    output_kind = NkbOutput_Static;
                } else if (value == "obj") {
                    output_kind = NkbOutput_Object;
                } else {
                    nkirc_diag_printError(
                        "invalid output kind `" nks_Fmt
                        "`. Possible values are `run`, `executable`, `shared`, `static`, `object`",
                        nks_Arg(value));
                    printErrorUsage();
                    return 1;
                }
            } else if (key == "-l" || key == "--link") {
                GET_VALUE;
                nksb_printf(&args, " -l" nks_Fmt, nks_Arg(value));
            } else if (key == "-c" || key == "--color") {
                GET_VALUE;
                if (value == "auto") {
                    nkirc_diag_init(NkIrcColor_Auto);
                } else if (value == "always") {
                    nkirc_diag_init(NkIrcColor_Always);
                } else if (value == "never") {
                    nkirc_diag_init(NkIrcColor_Never);
                } else {
                    nkirc_diag_printError(
                        "invalid color mode `" nks_Fmt "`. Possible values are `auto`, `always`, `never`",
                        nks_Arg(value));
                    printErrorUsage();
                    return 1;
                }
#ifdef ENABLE_LOGGING
                if (value == "auto") {
                    logger_opts.color_mode = NkLog_Color_Auto;
                } else if (value == "always") {
                    logger_opts.color_mode = NkLog_Color_Always;
                } else if (value == "never") {
                    logger_opts.color_mode = NkLog_Color_Never;
                }
            } else if (key == "-L" || key == "--loglevel") {
                GET_VALUE;
                if (value == "none") {
                    logger_opts.log_level = NkLog_None;
                } else if (value == "error") {
                    logger_opts.log_level = NkLog_Error;
                } else if (value == "warning") {
                    logger_opts.log_level = NkLog_Warning;
                } else if (value == "info") {
                    logger_opts.log_level = NkLog_Info;
                } else if (value == "debug") {
                    logger_opts.log_level = NkLog_Debug;
                } else if (value == "trace") {
                    logger_opts.log_level = NkLog_Trace;
                } else {
                    nkirc_diag_printError(
                        "invalid loglevel `" nks_Fmt
                        "`. Possible values are `none`, `error`, `warning`, `info`, "
                        "`debug`, `trace`",
                        nks_Arg(value));
                    printErrorUsage();
                    return 1;
                }
#endif // ENABLE_LOGGING
            } else if (key == "-h" || key == "--help") {
                NO_VALUE;
                help = true;
            } else if (key == "-v" || key == "--version") {
                NO_VALUE;
                version = true;
            } else if (key == "-g") {
                NO_VALUE;
                nksb_printf(&args, " -g");
            } else {
                nkirc_diag_printError("invalid argument `" nks_Fmt "`", nks_Arg(arg));
                printErrorUsage();
                return 1;
            }
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

    if (!in_file.data) {
        nkirc_diag_printError("no input file");
        printErrorUsage();
        return 1;
    }

    if (!out_file.data) {
        out_file = nk_cs2s("a.out");
    }

    NK_LOGGER_INIT(logger_opts);

    NkArena arena{};
    defer {
        nk_arena_free(&arena);
    };

    auto const c = nkirc_create(&arena);
    defer {
        nkirc_free(c);
    };

    int code;
    if (run) {
        code = nkir_run(c, in_file);
    } else {
        NkIrCompilerConfig compiler_config{
            .compiler_binary = {},
            .additional_flags = {},
            .output_filename = out_file,
            .output_kind = output_kind,
            .quiet = false,
        };

        auto compiler_path_buf = (char *)nk_arena_alloc(&arena, NK_MAX_PATH);
        int compiler_path_len = nk_getBinaryPath(compiler_path_buf, NK_MAX_PATH);
        if (compiler_path_len < 0) {
            nkirc_diag_printError("failed to get compiler binary path");
            return 1;
        }

        nks compiler_dir{compiler_path_buf, (size_t)compiler_path_len};
        nks_chop_by_delim_reverse(&compiler_dir, nk_path_separator);

        NkStringBuilder config_path{0, 0, 0, nk_arena_getAllocator(&arena)};
        nksb_printf(&config_path, nks_Fmt "%c" NK_BINARY_NAME ".conf", nks_Arg(compiler_dir), nk_path_separator);

        NK_LOG_DBG("config_path=`" nks_Fmt "`", nks_Arg(config_path));

        auto config = NkHashMap<nks, nks>::create(nk_arena_getAllocator(&arena));
        if (!readConfig(config, nk_arena_getAllocator(&arena), {nkav_init(config_path)})) {
            return 1;
        }

        auto c_compiler = config.find(nk_cs2s("c_compiler"));
        if (!c_compiler) {
            nkirc_diag_printError("`c_compiler` field is missing in the config");
            return 1;
        }
        NK_LOG_DBG("c_compiler=`" nks_Fmt "`", nks_Arg(*c_compiler));
        compiler_config.compiler_binary = *c_compiler;

        auto c_flags = config.find(nk_cs2s("c_flags"));
        if (c_flags) {
            NK_LOG_DBG("c_flags=`" nks_Fmt "`", nks_Arg(*c_flags));
            nksb_printf(&args, " " nks_Fmt, nks_Arg(*c_flags));
        }

        compiler_config.additional_flags = {nkav_init(args)};

        code = nkir_compile(c, in_file, compiler_config);
    }

#ifdef BUILD_WITH_EASY_PROFILER
    puts("press any key to exit");
    getchar();
#endif // BUILD_WITH_EASY_PROFILER

    return code;
}
