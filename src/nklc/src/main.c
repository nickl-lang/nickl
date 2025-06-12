#include "nkb/ir.h"
#include "nkl/common/diagnostics.h"
#include "nkl/core/nickl.h"
#include "ntk/allocator.h"
#include "ntk/atom.h"
#include "ntk/cli.h"
#include "ntk/common.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/profiler.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

static void printErrorUsage() {
    nk_printf(nk_file_getStream(nk_stderr()), "See `%s --help` for usage information\n", NK_BINARY_NAME);
}

static void printUsage() {
    printf(
        "Usage: " NK_BINARY_NAME
        " [options] file"
        "\nOptions:"
        "\n    -o, --output <file>                              Output file path"
        "\n    -k, --kind {run,exe,static,shared,archive,obj}   Output file kind"
        "\n    -c, --color {auto,always,never}                  Choose when to color output"
        "\n    -h, --help                                       Display this message and exit"
        "\n    -v, --version                                    Show version information"
#ifdef ENABLE_LOGGING
        "\nDeveloper options:"
        "\n    -t, --loglevel {none,error,warning,info,debug,trace}   Select logging level"
#endif // ENABLE_LOGGING
        "\n");
}

static void printVersion() {
    printf(NK_BINARY_NAME " " NK_BUILD_VERSION " " NK_BUILD_TIME "\n");
}

static void printDiag(NklState nkl) {
    NklError const *err = nkl_getErrors(nkl);
    while (err) {
        if (err->loc.file.size) {
            char cwd[NK_MAX_PATH];
            nk_getCwd(cwd, sizeof(cwd));

            NKSB_FIXED_BUFFER(file_nt, NK_MAX_PATH);
            nksb_printf(&file_nt, NKS_FMT, NKS_ARG(err->loc.file));
            nksb_appendNull(&file_nt);

            char relpath[NK_MAX_PATH];
            nk_relativePath(relpath, sizeof(relpath), file_nt.data, cwd);

            NklSourceLocation loc = err->loc;
            loc.file = nk_cs2s(relpath);

            // TODO: Avoid reading file again
            NkString text;
            if (nk_file_read(nk_default_allocator, err->loc.file, &text)) {
                nkl_diag_printErrorQuote(text, loc, NKS_FMT, NKS_ARG(err->msg));
                nk_free(nk_default_allocator, (void *)text.data, text.size);
            } else {
                nkl_diag_printErrorFile(loc, NKS_FMT, NKS_ARG(err->msg));
            }
        } else {
            nkl_diag_printError(NKS_FMT, NKS_ARG(err->msg));
        }

        err = err->next;
    }
}

typedef struct {
    NklState nkl;
    NkString in_file;
    NkString out_file;
    NklOutputKind out_kind;
    bool run;
} RunInfo;

static int run(RunInfo const info) {
    NklState const nkl = info.nkl;

    NklCompiler const com = nkl_newCompilerHost(nkl);

    // TODO: Hardcoded lib names
    nkl_addLibraryAlias(com, nk_cs2s("c"), nk_cs2s("libc.so.6"));
    nkl_addLibraryAlias(com, nk_cs2s("m"), nk_cs2s("libm.so.6"));
    nkl_addLibraryAlias(com, nk_cs2s("pthread"), nk_cs2s("libpthread.so.0"));

    NklModule const mod = nkl_newModule(com);

    if (!nkl_compileFile(mod, info.in_file)) {
        printDiag(nkl);
        return 1;
    }

    if (info.run) {
        if (!nkl_compileStringIr(mod, nk_cs2s("\n\
pub proc sayHello() {\n\
    call printf, (\"hello\\n\", ...) -> :i32\n\
    ret\n\
}\n\
"))) {
            printDiag(nkl);
            return 1;
        }

        void (*sayHello)(void) = nkl_getSymbolAddress(mod, nk_cs2s("sayHello"));
        if (!sayHello) {
            printDiag(nkl);
            return 1;
        }
        sayHello();

        void (*entry)(void) = nkl_getSymbolAddress(mod, nk_cs2s("_entry"));
        if (!entry) {
            printDiag(nkl);
            return 1;
        }
        entry();
    } else {
        if (!nkl_exportModule(mod, info.out_file, info.out_kind)) {
            printDiag(nkl);
            return 1;
        }
    }

    return 0;
}

static int parseArgsAndRun(char **argv) {
    RunInfo run_info = {
        .out_file = nk_cs2s("a.out"),
        .out_kind = NklOutput_Binary,
    };

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
            } else if (nks_equal(key, nk_cs2s("-o")) || nks_equal(key, nk_cs2s("--output"))) {
                GET_VALUE;
                run_info.out_file = val;
            } else if (nks_equal(key, nk_cs2s("-k")) || nks_equal(key, nk_cs2s("--kind"))) {
                GET_VALUE;
                if (nks_equal(val, nk_cs2s("run"))) {
                    run_info.run = true;
                } else if (nks_equal(val, nk_cs2s("exe"))) {
                    run_info.out_kind = NklOutput_Binary;
                } else if (nks_equal(val, nk_cs2s("static"))) {
                    run_info.out_kind = NklOutput_Static;
                } else if (nks_equal(val, nk_cs2s("shared"))) {
                    run_info.out_kind = NklOutput_Shared;
                } else if (nks_equal(val, nk_cs2s("archive"))) {
                    run_info.out_kind = NklOutput_Archiv;
                } else if (nks_equal(val, nk_cs2s("obj"))) {
                    run_info.out_kind = NklOutput_Object;
                } else {
                    nkl_diag_printError(
                        "invalid output kind `" NKS_FMT
                        "`. Possible values are `run`, `exe`, `static`, `shared`, `archive`, `obj`",
                        NKS_ARG(val));
                    printErrorUsage();
                    return 1;
                }
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
        } else if (!run_info.in_file.size) {
            run_info.in_file = val;
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

    if (!run_info.in_file.size) {
        nkl_diag_printError("no input file");
        printErrorUsage();
        return 1;
    }

    NK_LOG_INIT(log_opts);

    int ret_code = 0;

    NK_DEFER_LOOP(nk_atom_init(), nk_atom_init())
    NK_DEFER_LOOP(run_info.nkl = nkl_newState(), nkl_freeState(run_info.nkl)) {
        ret_code = run(run_info);
    }

    return ret_code;
}

int main(int NK_UNUSED argc, char **argv) {
    int ret_code = 0;

    NK_DEFER_LOOP(NK_PROF_START(NK_BINARY_NAME ".spall"), NK_PROF_FINISH())
    NK_DEFER_LOOP(NK_PROF_THREAD_ENTER(0, 32 * 1024 * 1024), NK_PROF_THREAD_LEAVE())
    NK_PROF_FUNC() {
        ret_code = parseArgsAndRun(argv);
    }

    return ret_code;
}
