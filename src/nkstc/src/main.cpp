#include "nkb/common.h"
#include "nkl/common/diagnostics.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/cli.h"
#include "ntk/dyn_array.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/os/file.h"
#include "ntk/profiler.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "stc.h"

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

    int code{};
    if (run) {
        code = nkst_run(in_file);
    } else {
        NkDynArray(NkString) additional_flags{NKDA_INIT(alloc)};

        if (add_debug_info) {
            nkda_append(&additional_flags, nk_cs2s("-g"));
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

        code = nkst_compile(
            in_file,
            {
                .compiler_binary = nk_cs2s("gcc"), // TODO: Hardcoded C compiler
                .additional_flags{NKS_INIT(additional_flags)},
                .output_filename = out_file,
                .output_kind = output_kind,
                .quiet = false,
            });
    }

    return code;
}
