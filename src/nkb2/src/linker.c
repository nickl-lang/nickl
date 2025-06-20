#include "linker.h"

#include "nkb/ir.h"
#include "ntk/arena.h"
#include "ntk/error.h"
#include "ntk/log.h"
#include "ntk/process.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(linker);

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

    if (kind == NkIrOutput_Archiv) {
        nksb_printf(&link_cmd, "ar rcs");
        nksb_printf(&link_cmd, " \"" NKS_FMT "\"", NKS_ARG(opts.out_file));
        nksb_printf(&link_cmd, " \"" NKS_FMT "\"", NKS_ARG(opts.obj_file));
    } else {
        // TODO: Do not depend on gcc for linking
        nksb_printf(&link_cmd, "gcc");

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
    }

    NK_LOG_INF("Linking: " NKS_FMT, NKS_ARG(link_cmd));

    i32 exit_code = 0;
    if (nk_exec(scratch, (NkString){NKS_INIT(link_cmd)}, NULL, NULL, NULL, &exit_code) < 0) {
        nk_error_printf("Failed to run the linker: %s", nk_getLastErrorString());
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
