#include "linker.h"

#include "nkb/ir.h"
#include "ntk/arena.h"
#include "ntk/error.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/pipe_stream.h"
#include "ntk/process.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(linker);

// TODO: Do not depend on gcc for finding files
static bool findFile(NkArena *scratch, NkString name, NkString *out_path) {
    NK_LOG_TRC("%s", __func__);

    NkStream out;
    NkPipeStream ps;
    if (!nk_pipe_streamOpenRead(
            (NkPipeStreamInfo){
                .ps = &ps,
                .scratch = scratch,
                .cmd = nk_tsprintf(scratch, "gcc -print-file-name=" NKS_FMT, NKS_ARG(name)),
                .opt_buf = {0},
                .quiet = false,
            },
            &out)) {
        // TODO: Report errors properly
        NK_LOG_ERR("Failed to find file `" NKS_FMT "`: %s", NKS_ARG(name), nk_getLastErrorString());
        return false;
    }

    NKSB_FIXED_BUFFER(path, NK_MAX_PATH);
    nksb_readFromStream(&path, out);
    path.size = nks_trimRight((NkString){NKS_INIT(path)}).size;
    nksb_appendNull(&path);

    if (nk_pipe_streamClose(&ps)) {
        // TODO: Report errors properly
        NK_LOG_ERR("Failed to find file `" NKS_FMT "`: %s", NKS_ARG(name), nk_getLastErrorString());
        return false;
    }

    char full_path[NK_MAX_PATH];
    if (nk_fullPath(full_path, path.data) < 0) {
        // TODO: Report errors properly
        NK_LOG_ERR("Failed to find file `" NKS_FMT "`: %s", NKS_ARG(name), nk_getLastErrorString());
        return false;
    }

    NK_LOG_INF("Found file `" NKS_FMT "`: %s", NKS_ARG(name), full_path);

    *out_path = nk_tsprintf(scratch, "%s", full_path);
    return true;
}

void nk_link(NkLikerOpts const opts) {
    NkStringBuilder link_cmd = {.alloc = nk_arena_getAllocator(opts.scratch)};
    switch (opts.out_kind) {
        case NkIrOutput_Static:
            nksb_printf(&link_cmd, "ar rcs");
            nksb_printf(&link_cmd, " \"" NKS_FMT "\"", NKS_ARG(opts.out_file));
            nksb_printf(&link_cmd, " \"" NKS_FMT "\"", NKS_ARG(opts.obj_file));
            break;

        case NkIrOutput_Shared: {
            NkString crti;
            NkString crtn;
            NkString crtbeginS;
            NkString crtendS;
            bool ok = true;
            ok &= findFile(opts.scratch, nk_cs2s("crti.o"), &crti);
            ok &= findFile(opts.scratch, nk_cs2s("crtn.o"), &crtn);
            ok &= findFile(opts.scratch, nk_cs2s("crtbeginS.o"), &crtbeginS);
            ok &= findFile(opts.scratch, nk_cs2s("crtendS.o"), &crtendS);
            if (!ok) {
                return;
            }

            nksb_printf(&link_cmd, "ld -shared");
            nksb_printf(&link_cmd, " -o \"" NKS_FMT "\"", NKS_ARG(opts.out_file));
            nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crti));
            nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crtbeginS));
            nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(opts.obj_file));
            nksb_printf(&link_cmd, " -lc");
            nksb_printf(&link_cmd, " --as-needed");
            nksb_printf(&link_cmd, " -lm");
            nksb_printf(&link_cmd, " --no-as-needed");
            nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crtendS));
            nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crtn));
            break;
        }

        case NkIrOutput_Binary: {
            NkString Scrt1;
            NkString crti;
            NkString crtn;
            NkString crtbeginS;
            NkString crtendS;
            bool ok = true;
            ok &= findFile(opts.scratch, nk_cs2s("Scrt1.o"), &Scrt1);
            ok &= findFile(opts.scratch, nk_cs2s("crti.o"), &crti);
            ok &= findFile(opts.scratch, nk_cs2s("crtn.o"), &crtn);
            ok &= findFile(opts.scratch, nk_cs2s("crtbeginS.o"), &crtbeginS);
            ok &= findFile(opts.scratch, nk_cs2s("crtendS.o"), &crtendS);
            if (!ok) {
                return;
            }

            nksb_printf(&link_cmd, "ld -pie");
            nksb_printf(&link_cmd, " -dynamic-linker /lib64/ld-linux-x86-64.so.2");
            nksb_printf(&link_cmd, " -o \"" NKS_FMT "\"", NKS_ARG(opts.out_file));
            nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(Scrt1));
            nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crti));
            nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crtbeginS));
            nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(opts.obj_file));
            nksb_printf(&link_cmd, " -lc");
            nksb_printf(&link_cmd, " --as-needed");
            nksb_printf(&link_cmd, " -lm");
            nksb_printf(&link_cmd, " --no-as-needed");
            nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crtendS));
            nksb_printf(&link_cmd, " " NKS_FMT, NKS_ARG(crtn));
            break;
        }

        case NkIrOutput_None:
        case NkIrOutput_Object:
            nk_assert(!"unreachable");
            return;
    }

    NK_LOG_INF("Linking: " NKS_FMT, NKS_ARG(link_cmd));

    if (nk_exec(opts.scratch, (NkString){NKS_INIT(link_cmd)}, NULL, NULL, NULL, NULL)) {
        // TODO: Report errors properly
        NK_LOG_ERR("%s", nk_getLastErrorString());
        return;
    }
}
