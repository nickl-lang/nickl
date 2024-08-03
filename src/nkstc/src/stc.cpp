#include "stc.h"

#include "nkl/common/diagnostics.h"
#include "nkl/core/compiler.h"
#include "ntk/atom.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/string.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(nkstc);

void printDiag(NklState nkl, NklCompiler c) {
    auto error = nkl_getCompileErrorList(c);
    while (error) {
        if (error->file) {
            char cwd[NK_MAX_PATH];
            nk_getCwd(cwd, sizeof(cwd));
            char relpath[NK_MAX_PATH];
            nk_relativePath(relpath, sizeof(relpath), nk_atom2cs(error->file), cwd);

            auto src = nkl_getSource(nkl, error->file);
            nkl_diag_printErrorQuote(
                src->text,
                {
                    .file = nk_cs2s(relpath),
                    .lin = error->token->lin,
                    .col = error->token->col,
                    .len = error->token->len,
                },
                NKS_FMT,
                NKS_ARG(error->msg));
        } else {
            nkl_diag_printError(NKS_FMT, NKS_ARG(error->msg));
        }

        error = error->next;
    }
}

} // namespace

int nkst_compile(NklState nkl, NkString in_file, NkIrCompilerConfig conf) {
    NK_LOG_TRC("%s", __func__);

    auto c = nkl_createCompiler(nkl, {});
    defer {
        nkl_freeCompiler(c);
    };

    auto m = nkl_createModule(c);
    if (!nkl_compileFile(m, in_file)) {
        printDiag(nkl, c);
        return 1;
    }

    if (!nkl_writeModule(m, conf)) {
        printDiag(nkl, c);
        return 1;
    }

    return 0;
}

int nkst_run(NklState nkl, NkString in_file) {
    NK_LOG_TRC("%s", __func__);

    auto c = nkl_createCompiler(nkl, {});
    defer {
        nkl_freeCompiler(c);
    };

    auto m = nkl_createModule(c);
    if (!nkl_compileFile(m, in_file)) {
        printDiag(nkl, c);
        return 1;
    }

    if (!nkl_runModule(m)) {
        printDiag(nkl, c);
        return 1;
    }

    return 0;
}
