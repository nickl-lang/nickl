#include "stc.h"

#include "lexer.h"
#include "nkl/common/diagnostics.h"
#include "nkl/core/compiler.h"
#include "nkl/core/nickl.h"
#include "ntk/atom.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/string.h"
#include "ntk/utils.h"
#include "parser.h"

namespace {

NK_LOG_USE_SCOPE(nkstc);

NklTokenArray lexer_proc(NklState /*nkl*/, NkAllocator alloc, NkAtom file, NkString text) {
    return nkst_lex(alloc, file, text);
}

NklAstNodeArray parser_proc(NklState /*nkl*/, NkAllocator alloc, NkAtom file, NkString text, NklTokenArray tokens) {
    return nkst_parse(alloc, file, text, tokens);
}

} // namespace

int nkst_compile(NkString in_file) {
    NK_LOG_TRC("%s", __func__);

    auto nkl = nkl_state_create(lexer_proc, parser_proc);
    defer {
        nkl_state_free(nkl);
    };

    auto c = nkl_createCompiler(nkl, {});
    defer {
        nkl_freeCompiler(c);
    };

    auto m = nkl_createModule(c);
    nkl_compileFile(m, in_file);

    auto error = nkl_getCompileErrorList(c);
    if (error) {
        while (error) {
            if (error->file != NK_ATOM_INVALID) {
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
        return 1;
    }

    nkl_writeModule(m, nk_cs2s("a.out"));

    return 0;
}
