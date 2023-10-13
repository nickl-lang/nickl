#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "config.hpp"
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

bool eql(char const *lhs, char const *rhs) {
    return lhs && rhs && strcmp(lhs, rhs) == 0;
}

NK_LOG_USE_SCOPE(main);

} // namespace

int main(int argc, char const *const *argv) {
#ifdef BUILD_WITH_EASY_PROFILER
    EASY_PROFILER_ENABLE;
    ::profiler::startListen(EASY_PROFILER_PORT);
#endif // BUILD_WITH_EASY_PROFILER

    char const *in_file = nullptr;
    char const *out_file = nullptr;
    bool run = false;
    NkbOutputKind output_kind = NkbOutput_Executable;

    NkStringBuilder args{};
    defer {
        nksb_free(&args);
    };

    bool help = false;
    bool version = false;

    NkIrcOptions nkirc_opts{};

#ifdef ENABLE_LOGGING
    NkLoggerOptions logger_opts{};
    logger_opts.log_level = NkLog_Error;
#endif // ENABLE_LOGGING

    for (int i = 1; i < argc;) {
        auto const arg = argv[i++];

        if (arg[0] == '-') {
            if (eql(arg, "-h") || eql(arg, "--help")) {
                help = true;
            } else if (eql(arg, "-v") || eql(arg, "--version")) {
                version = true;
            } else if (eql(arg, "-o") || eql(arg, "--output")) {
                if (!argv[i]) {
                    fprintf(stderr, "error: argument required\n");
                    printErrorUsage();
                    return 1;
                }
                out_file = argv[i++];
            } else if (eql(arg, "-k") || eql(arg, "--kind")) {
                if (!argv[i]) {
                    fprintf(stderr, "error: argument required\n");
                    printErrorUsage();
                    return 1;
                }
                auto const output_kind_str = argv[i++];
                if (eql(output_kind_str, "run")) {
                    run = true;
                } else if (eql(output_kind_str, "exe")) {
                    output_kind = NkbOutput_Executable;
                } else if (eql(output_kind_str, "shared")) {
                    output_kind = NkbOutput_Shared;
                } else if (eql(output_kind_str, "static")) {
                    output_kind = NkbOutput_Static;
                } else if (eql(output_kind_str, "obj")) {
                    output_kind = NkbOutput_Object;
                } else {
                    fprintf(
                        stderr,
                        "error: invalid output kind `%s`. Possible values are `executable`, `shared`, `static`, "
                        "`object`\n",
                        output_kind_str);
                    printErrorUsage();
                    return 1;
                }
            } else if (eql(arg, "-l") || eql(arg, "--link")) {
                if (!argv[i]) {
                    fprintf(stderr, "error: argument required\n");
                    printErrorUsage();
                    return 1;
                }
                auto const arg_str = nk_cs2s(argv[i++]);
                nksb_printf(&args, " -l" nks_Fmt, nks_Arg(arg_str));
            } else if (eql(arg, "-g")) {
                nksb_printf(&args, " -g");
            } else if (eql(arg, "-c") || eql(arg, "--color")) {
                if (!argv[i]) {
                    fprintf(stderr, "error: argument required\n");
                    printErrorUsage();
                    return 1;
                }
                auto const color_mode_str = argv[i++];
                if (eql(color_mode_str, "auto")) {
                    nkirc_opts.color_policy = NkIrcColor_Auto;
                } else if (eql(color_mode_str, "always")) {
                    nkirc_opts.color_policy = NkIrcColor_Always;
                } else if (eql(color_mode_str, "never")) {
                    nkirc_opts.color_policy = NkIrcColor_Never;
                } else {
                    fprintf(
                        stderr,
                        "error: invalid color mode `%s`. Possible values are `auto`, `always`, `never`\n",
                        color_mode_str);
                    printErrorUsage();
                    return 1;
                }
#ifdef ENABLE_LOGGING
                if (eql(color_mode_str, "auto")) {
                    logger_opts.color_mode = NkLog_Color_Auto;
                } else if (eql(color_mode_str, "always")) {
                    logger_opts.color_mode = NkLog_Color_Always;
                } else if (eql(color_mode_str, "never")) {
                    logger_opts.color_mode = NkLog_Color_Never;
                }
            } else if (eql(arg, "-L") || eql(arg, "--loglevel")) {
                if (!argv[i]) {
                    fprintf(stderr, "error: argument required\n");
                    printErrorUsage();
                    return 1;
                }
                auto const log_level_str = argv[i++];
                if (eql(log_level_str, "none")) {
                    logger_opts.log_level = NkLog_None;
                } else if (eql(log_level_str, "error")) {
                    logger_opts.log_level = NkLog_Error;
                } else if (eql(log_level_str, "warning")) {
                    logger_opts.log_level = NkLog_Warning;
                } else if (eql(log_level_str, "info")) {
                    logger_opts.log_level = NkLog_Info;
                } else if (eql(log_level_str, "debug")) {
                    logger_opts.log_level = NkLog_Debug;
                } else if (eql(log_level_str, "trace")) {
                    logger_opts.log_level = NkLog_Trace;
                } else {
                    fprintf(
                        stderr,
                        "error: invalid loglevel `%s`. Possible values are `none`, `error`, `warning`, `info`, "
                        "`debug`, `trace`\n",
                        log_level_str);
                    printErrorUsage();
                    return 1;
                }
#endif // ENABLE_LOGGING
            } else {
                fprintf(stderr, "error: invalid argument `%s`\n", arg);
                printErrorUsage();
                return 1;
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

    if (!out_file) {
        out_file = "a.out";
    }

    NK_LOGGER_INIT(logger_opts);

    auto const c = nkirc_create(nkirc_opts);
    defer {
        nkirc_free(c);
    };

    int code;
    if (run) {
        code = nkir_run(c, nk_cs2s(in_file));
    } else {
        NkArena arena{};
        defer {
            nk_arena_free(&arena);
        };
        NkIrCompilerConfig compiler_config{
            .compiler_binary = {},
            .additional_flags = {},
            .output_filename = nk_cs2s(out_file),
            .output_kind = output_kind,
            .quiet = false,
        };

        auto compiler_path_buf = (char *)nk_arena_alloc(&arena, NK_MAX_PATH);
        int compiler_path_len = nk_getBinaryPath(compiler_path_buf, NK_MAX_PATH);
        if (compiler_path_len < 0) {
            fprintf(stderr, "error: failed to get compiler binary path\n");
            return 1;
        }

        nks compiler_dir{compiler_path_buf, (size_t)compiler_path_len};
        nks_chop_by_delim_reverse(&compiler_dir, nk_path_separator);

        NkStringBuilder config_path{0, 0, 0, nk_arena_getAllocator(&arena)};
        nksb_printf(&config_path, nks_Fmt "%c" NK_BINARY_NAME ".conf", nks_Arg(compiler_dir), nk_path_separator);

        NK_LOG_DBG("config_path=`" nks_Fmt "`", nks_Arg(config_path));

        auto config = NkHashMap<nks, nks>::create(nk_arena_getAllocator(&arena));
        if (!readConfig(config, nk_arena_getAllocator(&arena), {nkav_init(config_path)})) {
            fprintf(stderr, "error: failed to read compiler config\n");
            return 1;
        }

        auto c_compiler = config.find(nk_cs2s("c_compiler"));
        if (!c_compiler) {
            fprintf(stderr, "error: `c_compiler` field is missing in the config\n");
            return 1;
        }
        compiler_config.compiler_binary = *c_compiler;

        auto c_flags = config.find(nk_cs2s("c_flags"));
        if (c_flags) {
            nksb_printf(&args, " " nks_Fmt, nks_Arg(*c_flags));
        }

        compiler_config.additional_flags = {nkav_init(args)};

        code = nkir_compile(c, nk_cs2s(in_file), compiler_config);
    }

#ifdef BUILD_WITH_EASY_PROFILER
    puts("press any key to exit");
    getchar();
#endif // BUILD_WITH_EASY_PROFILER

    return code;
}
