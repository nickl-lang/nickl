#include "irc.h"
#include "nkb/common.h"
#include "nkl/common/config.h"
#include "nkl/common/diagnostics.h"
#include "ntk/allocator.h"
#include "ntk/atom.h"
#include "ntk/cli.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

namespace {

void printErrorUsage() {
    nk_stream_printf(nk_file_getStream(nk_stderr()), "See `%s --help` for usage information\n", NK_BINARY_NAME);
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
    NK_PROF_START(NK_BINARY_NAME ".spall");
    NK_PROF_THREAD_ENTER(0, 32 * 1024 * 1024);
    defer {
        NK_PROF_THREAD_LEAVE();
        NK_PROF_FINISH();
    };

    NK_PROF_FUNC();

    NkString in_file{};
    NkString out_file{};
    bool run = false;
    NkbOutputKind output_kind = NkbOutput_Executable;

    bool help = false;
    bool version = false;
    bool add_debug_info = false;
    bool enable_asan = false;

    NkArena arena{};
    NkAllocator alloc = nk_arena_getAllocator(&arena);
    defer {
        nk_arena_free(&arena);
    };

    NkDynArray(NkString) link{NKDA_INIT(alloc)};
    NkDynArray(NkString) link_dirs{NKDA_INIT(alloc)};
    NkString opt{};

#ifdef ENABLE_LOGGING
    NkLogOptions log_opts{};
    log_opts.log_level = NkLogLevel_Warning;
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
                        "invalid output kind `" NKS_FMT
                        "`. Possible values are `run`, `exe`, `shared`, `static`, `object`",
                        NKS_ARG(val));
                    printErrorUsage();
                    return 1;
                }
            } else if (key == "-l") {
                GET_VALUE;
                nkda_append(&link, val);
            } else if (key == "-L") {
                GET_VALUE;
                nkda_append(&link_dirs, val);
            } else if (key == "-g") {
                NO_VALUE;
                add_debug_info = true;
            } else if (key == "-O") {
                GET_VALUE;
                opt = val;
            } else if (key == "--asan") {
                NO_VALUE;
                enable_asan = true;
            } else if (key == "-c" || key == "--color") {
                GET_VALUE;
                if (val == "auto") {
                    nkl_diag_init(NklColorPolicy_Auto);
                } else if (val == "always") {
                    nkl_diag_init(NklColorPolicy_Always);
                } else if (val == "never") {
                    nkl_diag_init(NklColorPolicy_Never);
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

    if (!out_file.data) {
        out_file = nk_cs2s("a.out");
    }

    NK_LOG_INIT(log_opts);

    nk_atom_init();
    defer {
        nk_atom_deinit();
    };

    auto compiler_path_buf = (char *)nk_arena_alloc(&arena, NK_MAX_PATH);
    int compiler_path_len = nk_getBinaryPath(compiler_path_buf, NK_MAX_PATH);
    if (compiler_path_len < 0) {
        nkl_diag_printError("failed to get compiler binary path");
        return 1;
    }

    NkString compiler_dir{compiler_path_buf, (usize)compiler_path_len};
    nks_chopByDelimReverse(&compiler_dir, NK_PATH_SEPARATOR);

    NkStringBuilder config_path{NKSB_INIT(alloc)};
    nksb_printf(&config_path, NKS_FMT "%c" NK_BINARY_NAME ".conf", NKS_ARG(compiler_dir), NK_PATH_SEPARATOR);

    NK_LOG_DBG("config_path=`" NKS_FMT "`", NKS_ARG(config_path));

    nks_config config{};
    config.alloc = alloc;
    if (!readConfig(&config, {NKS_INIT(config_path)})) {
        return 1;
    }

    NkIrcConfig irc_conf{};

    auto ptr_size = nks_config_find(&config, nk_cs2s("usize"));
    if (ptr_size) {
        NK_LOG_DBG("usize=`" NKS_FMT "`", NKS_ARG(ptr_size->val));
        char *endptr = NULL;
        irc_conf.ptr_size = strtol(ptr_size->val.data, &endptr, 10);
        if (endptr != ptr_size->val.data + ptr_size->val.size || !irc_conf.ptr_size ||
            !nk_isZeroOrPowerOf2(irc_conf.ptr_size)) {
            nkl_diag_printError("invalid usize in config: `" NKS_FMT "`", NKS_ARG(ptr_size->val));
            return 1;
        }
    }

    auto libc_name = nks_config_find(&config, nk_cs2s("libc_name"));
    if (libc_name) {
        NK_LOG_DBG("libc_name=`" NKS_FMT "`", NKS_ARG(libc_name->val));
        irc_conf.libc_name = nk_s2atom(libc_name->val);
    }

    auto libm_name = nks_config_find(&config, nk_cs2s("libm_name"));
    if (libm_name) {
        NK_LOG_DBG("libm_name=`" NKS_FMT "`", NKS_ARG(libm_name->val));
        irc_conf.libm_name = nk_s2atom(libm_name->val);
    }

    auto libpthread_name = nks_config_find(&config, nk_cs2s("libpthread_name"));
    if (libpthread_name) {
        NK_LOG_DBG("libpthread_name=`" NKS_FMT "`", NKS_ARG(libpthread_name->val));
        irc_conf.libpthread_name = nk_s2atom(libpthread_name->val);
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
        NK_LOG_DBG("c_compiler=`" NKS_FMT "`", NKS_ARG(c_compiler->val));

        NkDynArray(NkString) additional_flags{NKDA_INIT(alloc)};

        auto c_flags = nks_config_find(&config, nk_cs2s("c_flags"));
        if (c_flags) {
            NK_LOG_DBG("c_flags=`" NKS_FMT "`", NKS_ARG(c_flags->val));
            nkda_append(&additional_flags, c_flags->val);
        }

        if (add_debug_info || enable_asan) {
            nkda_append(&additional_flags, nk_cs2s("-g"));
        }

        if (enable_asan) {
            nkda_append(&additional_flags, nk_cs2s("-fsanitize=address"));
        }

        for (auto dir : nk_iterate(link_dirs)) {
            NkStringBuilder sb{NKSB_INIT(alloc)};
            nksb_printf(&sb, "-L" NKS_FMT, NKS_ARG(dir));
            nkda_append(&additional_flags, NkString{NKS_INIT(sb)});
        }

        for (auto lib : nk_iterate(link)) {
            NkStringBuilder sb{NKSB_INIT(alloc)};
            nksb_printf(&sb, "-l" NKS_FMT, NKS_ARG(lib));
            nkda_append(&additional_flags, NkString{NKS_INIT(sb)});
        }

        if (opt.size) {
            NkStringBuilder sb{NKSB_INIT(alloc)};
            nksb_printf(&sb, "-O" NKS_FMT, NKS_ARG(opt));
            nkda_append(&additional_flags, NkString{NKS_INIT(sb)});
        }

        code = nkir_compile(
            c,
            in_file,
            {
                .compiler_binary = c_compiler->val,
                .additional_flags{NKS_INIT(additional_flags)},
                .output_filename = out_file,
                .output_kind = output_kind,
                .quiet = false,
            });
    }

    return code;
}
