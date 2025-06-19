#include "linker.h"

#include "nkb/ir.h"
#include "ntk/arena.h"
#include "ntk/error.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/pipe_stream.h"
#include "ntk/process.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(linker);

// TODO: Do not depend on gcc for finding files?
static bool findFile(NkArena *scratch, char const *name, NkString *out_path) {
    NK_LOG_TRC("%s", __func__);

    NK_PROF_FUNC_BEGIN();

    NkStream out;
    NkPipeStream ps;
    if (!nk_pipe_streamOpenRead(
            (NkPipeStreamInfo){
                .ps = &ps,
                .scratch = scratch,
                .cmd = nk_tsprintf(scratch, "gcc -print-file-name=%s", name),
                .opt_buf = {0},
                .quiet = false,
            },
            &out)) {
        nk_error_printf("Failed to find file `%s`: %s", name, nk_getLastErrorString());
        NK_PROF_END();
        return false;
    }

    NKSB_FIXED_BUFFER(path, NK_MAX_PATH);
    nksb_readFromStream(&path, out);
    path.size = nks_trimRight((NkString){NKS_INIT(path)}).size;
    nksb_appendNull(&path);

    if (nk_pipe_streamClose(&ps)) {
        nk_error_printf("Failed to find file `%s`: %s", name, nk_getLastErrorString());
        NK_PROF_END();
        return false;
    }

    char full_path[NK_MAX_PATH];
    if (nk_fullPath(full_path, path.data) < 0) {
        nk_error_printf("Failed to find file `%s`: %s", name, nk_getLastErrorString());
        NK_PROF_END();
        return false;
    }

    NK_LOG_INF("Found file `%s`: %s", name, full_path);

    *out_path = nk_tsprintf(scratch, "%s", full_path);
    NK_PROF_END();
    return true;
}

#define TRY(EXPR)          \
    do {                   \
        if (!(EXPR)) {     \
            NK_PROF_END(); \
            return 0;      \
        }                  \
    } while (0)

bool nk_link(NkLikerOpts const opts) {
    NK_LOG_TRC("%s", __func__);

    NK_PROF_FUNC_BEGIN();

    NkArena *scratch = opts.scratch;
    NkIrOutputKind kind = opts.out_kind;

    if (kind == NkIrOutput_None || kind == NkIrOutput_Object) {
        NK_PROF_END();
        return false;
    }

    NkStringBuilder link_cmd = {.alloc = nk_arena_getAllocator(scratch)};

    char const *linker = "ld"; // TODO: Hardcoded linker name

    if (kind == NkIrOutput_Archiv) {
        nksb_printf(&link_cmd, "ar rcs");
        nksb_printf(&link_cmd, " \"" NKS_FMT "\"", NKS_ARG(opts.out_file));
        nksb_printf(&link_cmd, " \"" NKS_FMT "\"", NKS_ARG(opts.obj_file));
    } else {
#if defined(__APPLE__)
        // TODO: Falling back to clang on darwin
        nksb_printf(&link_cmd, "clang");

        switch (kind) {
            case NkIrOutput_Binary:
                break;

            case NkIrOutput_Static:
                nksb_printf(&link_cmd, " -static");
                break;

            case NkIrOutput_Shared:
                nksb_printf(&link_cmd, " -shared");
                break;

            case NkIrOutput_None:
            case NkIrOutput_Archiv:
            case NkIrOutput_Object:
                nk_assert(!"unreachable");
                break;
        }

        nksb_printf(&link_cmd, " -o \"" NKS_FMT "\"", NKS_ARG(opts.out_file));
        nksb_printf(&link_cmd, " \"" NKS_FMT "\"", NKS_ARG(opts.obj_file));
        nksb_printf(&link_cmd, " -lm"); // TODO: Hardcoded libm
#else
        bool const is_static = kind == NkIrOutput_Static;
        bool const is_exe = kind == NkIrOutput_Binary || is_static;
        bool const is_dynamic = !is_static;

        NkString crt1;
        NkString crti;
        NkString crtn;
        NkString crtbegin;
        NkString crtend;
        if (is_exe) {
            TRY(findFile(scratch, is_dynamic ? "Scrt1.o" : "crt1.o", &crt1));
        }
        TRY(findFile(scratch, "crti.o", &crti));
        TRY(findFile(scratch, "crtn.o", &crtn));
        TRY(findFile(scratch, is_dynamic ? "crtbeginS.o" : "crtbeginT.o", &crtbegin));
        TRY(findFile(scratch, is_dynamic ? "crtendS.o" : "crtend.o", &crtend));

        NkString const toolchain_dir = nk_path_getParent(crtbegin);

        nksb_printf(&link_cmd, "%s", linker);
        nksb_printf(&link_cmd, is_exe ? (is_dynamic ? " -pie" : " -static") : " -shared");
        if (is_exe && is_dynamic) {
            nksb_printf(&link_cmd, " -dynamic-linker /lib64/ld-linux-x86-64.so.2");
        }
        nksb_printf(&link_cmd, " -o \"" NKS_FMT "\"", NKS_ARG(opts.out_file));
        if (is_exe) {
            nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crt1));
        }
        nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crti));
        nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crtbegin));
        nksb_printf(&link_cmd, " -L" NKS_FMT, NKS_ARG(toolchain_dir));
        nksb_printf(&link_cmd, " -L/usr/lib64");
        nksb_printf(&link_cmd, " -L/usr/lib");
        nksb_printf(&link_cmd, " -L/lib64");
        nksb_printf(&link_cmd, " -L/lib");
        nksb_printf(&link_cmd, " \"" NKS_FMT "\"", NKS_ARG(opts.obj_file));
        if (is_static) {
            nksb_printf(&link_cmd, " --start-group");
        }
        nksb_printf(&link_cmd, " -lc");
        nksb_printf(&link_cmd, " -lgcc");
        if (is_static) {
            nksb_printf(&link_cmd, " -lgcc_eh");
            nksb_printf(&link_cmd, " --end-group");
        }
        nksb_printf(&link_cmd, " --as-needed");
        if (is_dynamic) {
            nksb_printf(&link_cmd, " -lgcc_s");
        }
        nksb_printf(&link_cmd, " -lm"); // TODO: Hardcoded libm
        nksb_printf(&link_cmd, " --no-as-needed");
        nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crtend));
        nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crtn));
#endif
    }

    NK_LOG_INF("Linking: " NKS_FMT, NKS_ARG(link_cmd));

    i32 exit_code = 0;
    if (nk_exec(scratch, (NkString){NKS_INIT(link_cmd)}, NULL, NULL, NULL, &exit_code) < 0) {
        nk_error_printf("Failed to run the linker `%s`: %s", linker, nk_getLastErrorString());
        NK_PROF_END();
        return false;
    }
    if (exit_code) {
        nk_error_printf("Linker returned nonzero exit code");
        NK_PROF_END();
        return false;
    }

    NK_PROF_END();
    return true;
}
