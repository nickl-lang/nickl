#include "irc.h"
#include "nkb/common.h"
#include "nkb/ir.h"
#include "nkl/common/config.h"
#include "nkl/common/diagnostics.h"
#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/cli.h"
#include "ntk/common.h"
#include "ntk/file.h"
#include "ntk/hash_map.hpp"
#include "ntk/logger.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/sys/path.h"
#include "ntk/utils.h"

namespace {

void printErrorUsage() {
    nk_printf(nk_file_getStream(nk_stderr()), "See `%s --help` for usage information\n", NK_BINARY_NAME);
}

void printUsage() {
    printf("Usage: " NK_BINARY_NAME
           " [options] file"
           "\nOptions:"
           "\n    -o, --output <file>                      Output file path"
           "\n    -k, --kind {run,exe,shared,static,obj}   Output file kind"
           "\n    -l <lib>                                 Link the library <lib>"
           "\n    -L <dir>                                 Search dir for linked libraries"
           "\n    -g                                       Add debug information"
           "\n    -c, --color {auto,always,never}          Choose when to color output"
           "\n    -h, --help                               Display this message and exit"
           "\n    -v, --version                            Show version information"
#ifdef ENABLE_LOGGING
           "\nDeveloper options:"
           "\n    -t, --loglevel {none,error,warning,info,debug,trace}   Select logging level"
#endif // ENABLE_LOGGING
           "\n");
}

void printVersion() {
    printf(NK_BINARY_NAME " " NK_BUILD_VERSION " " NK_BUILD_TIME "\n");
}

NK_LOG_USE_SCOPE(main);

} // namespace

int main(int /*argc*/, char const *const *argv) {
    ProfInit(NK_BINARY_NAME ".spall");
    ProfThreadInit(0, 32 * 1024 * 1024);
    defer {
        ProfThreadExit();
        ProfExit();
    };

    ProfFunc();

    nks in_file{};
    nks out_file{};
    bool run = false;
    NkbOutputKind output_kind = NkbOutput_Executable;

    bool help = false;
    bool version = false;
    bool add_debug_info = false;

    NkArena arena{};
    NkAllocator alloc = nk_arena_getAllocator(&arena);
    defer {
        nk_arena_free(&arena);
    };

    nkar_type(nks) link{0, 0, 0, alloc};
    nkar_type(nks) link_dirs{0, 0, 0, alloc};
    nks opt{};

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
            } else if (key == "-o" || key == "--output") {
                GET_VALUE;
                out_file = val;
            } else if (key == "-k" || key == "--kind") {
                GET_VALUE;
                if (val == "run") {
                    run = true;
                } else if (val == "exe") {
                    output_kind = NkbOutput_Executable;
                } else if (val == "shared") {
                    output_kind = NkbOutput_Shared;
                } else if (val == "static") {
                    output_kind = NkbOutput_Static;
                } else if (val == "obj") {
                    output_kind = NkbOutput_Object;
                } else {
                    nkl_diag_printError(
                        "invalid output kind `" nks_Fmt
                        "`. Possible values are `run`, `executable`, `shared`, `static`, `object`",
                        nks_Arg(val));
                    printErrorUsage();
                    return 1;
                }
            } else if (key == "-l") {
                GET_VALUE;
                nkar_append(&link, val);
            } else if (key == "-L") {
                GET_VALUE;
                nkar_append(&link_dirs, val);
            } else if (key == "-g") {
                NO_VALUE;
                add_debug_info = true;
            } else if (key == "-O") {
                GET_VALUE;
                opt = val;
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

    if (!out_file.data) {
        out_file = nk_cs2s("a.out");
    }

    NK_LOGGER_INIT(logger_opts);

    auto compiler_path_buf = (char *)nk_arena_alloc(&arena, NK_MAX_PATH);
    int compiler_path_len = nk_getBinaryPath(compiler_path_buf, NK_MAX_PATH);
    if (compiler_path_len < 0) {
        nkl_diag_printError("failed to get compiler binary path");
        return 1;
    }

    nks compiler_dir{compiler_path_buf, (usize)compiler_path_len};
    nks_chop_by_delim_reverse(&compiler_dir, nk_path_separator);

    NkStringBuilder config_path{0, 0, 0, alloc};
    nksb_printf(&config_path, nks_Fmt "%c" NK_BINARY_NAME ".conf", nks_Arg(compiler_dir), nk_path_separator);

    NK_LOG_DBG("config_path=`" nks_Fmt "`", nks_Arg(config_path));

    nks_config config{};
    config.alloc = alloc;
    if (!readConfig(&config, {nkav_init(config_path)})) {
        return 1;
    }

    NkIrcConfig irc_conf{};

    auto ptr_size = nks_config_find(&config, nk_cs2s("usize"));
    if (ptr_size) {
        NK_LOG_DBG("usize=`" nks_Fmt "`", nks_Arg(ptr_size->val));
        char *endptr = NULL;
        irc_conf.ptr_size = strtol(ptr_size->val.data, &endptr, 10);
        if (endptr != ptr_size->val.data + ptr_size->val.size || !irc_conf.ptr_size ||
            !isZeroOrPowerOf2(irc_conf.ptr_size)) {
            nkl_diag_printError("invalid usize in config: `" nks_Fmt "`", nks_Arg(ptr_size->val));
            return 1;
        }
    }

    auto libc_name = nks_config_find(&config, nk_cs2s("libc_name"));
    if (libc_name) {
        NK_LOG_DBG("libc_name=`" nks_Fmt "`", nks_Arg(libc_name->val));
        irc_conf.libc_name = s2nkid(libc_name->val);
    }

    auto libm_name = nks_config_find(&config, nk_cs2s("libm_name"));
    if (libm_name) {
        NK_LOG_DBG("libm_name=`" nks_Fmt "`", nks_Arg(libm_name->val));
        irc_conf.libm_name = s2nkid(libm_name->val);
    }

    auto libpthread_name = nks_config_find(&config, nk_cs2s("libpthread_name"));
    if (libpthread_name) {
        NK_LOG_DBG("libpthread_name=`" nks_Fmt "`", nks_Arg(libpthread_name->val));
        irc_conf.libpthread_name = s2nkid(libpthread_name->val);
    }

    auto const c = nkirc_create(&arena, irc_conf);
    defer {
        nkirc_free(c);
    };

    int code;
    if (run) {
        code = nkir_run(c, in_file);
    } else {
        auto c_compiler = nks_config_find(&config, nk_cs2s("c_compiler"));
        if (!c_compiler) {
            nkl_diag_printError("`c_compiler` field is missing in the config");
            return 1;
        }
        NK_LOG_DBG("c_compiler=`" nks_Fmt "`", nks_Arg(c_compiler->val));

        nkar_type(nks) additional_flags{0, 0, 0, alloc};

        auto c_flags = nks_config_find(&config, nk_cs2s("c_flags"));
        if (c_flags) {
            NK_LOG_DBG("c_flags=`" nks_Fmt "`", nks_Arg(c_flags->val));
            nkar_append(&additional_flags, c_flags->val);
        }

        if (add_debug_info) {
            nkar_append(&additional_flags, nk_cs2s("-g"));
        }

        for (auto dir : nk_iterate(link_dirs)) {
            NkStringBuilder sb{0, 0, 0, alloc};
            nksb_printf(&sb, "-L" nks_Fmt, nks_Arg(dir));
            nkar_append(&additional_flags, nks{nkav_init(sb)});
        }

        for (auto lib : nk_iterate(link)) {
            NkStringBuilder sb{0, 0, 0, alloc};
            nksb_printf(&sb, "-l" nks_Fmt, nks_Arg(lib));
            nkar_append(&additional_flags, nks{nkav_init(sb)});
        }

        if (opt.size) {
            NkStringBuilder sb{0, 0, 0, alloc};
            nksb_printf(&sb, "-O" nks_Fmt, nks_Arg(opt));
            nkar_append(&additional_flags, nks{nkav_init(sb)});
        }

        code = nkir_compile(
            c,
            in_file,
            {
                .compiler_binary = c_compiler->val,
                .additional_flags{nkav_init(additional_flags)},
                .output_filename = out_file,
                .output_kind = output_kind,
                .quiet = false,
            });
    }

    return code;
}
