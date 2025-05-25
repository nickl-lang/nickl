#include "nkl/core/nickl.h"

#include "ir_parser.h"
#include "nickl_impl.h"
#include "nkl/common/ast.h"
#include "nkl/common/diagnostics.h"
#include "nkl/common/token.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/string.h"

NK_LOG_USE_SCOPE(nickl);

NklState nkl_newState(void) {
    NK_LOG_TRC("%s", __func__);

    NkArena arena = {0};
    NklState nkl = nk_arena_allocT(&arena, NklState_T);
    *nkl = (NklState_T){
        .arena = arena,
        .error = NULL,
    };
    return nkl;
}

void nkl_freeState(NklState nkl) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(nkl && "state is null");

    NkArena arena = nkl->arena;
    nk_arena_free(&arena);
}

void nkl_pushState(NklState nkl) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(nkl && "state is null");

    nkl->next = s_nkl;
    s_nkl = nkl;
}

void nkl_popState(void) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(s_nkl && "no active state");
    s_nkl = s_nkl->next;
}

NklCompiler nkl_newCompiler(NklTargetTriple target) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(s_nkl && "no active state");
    NklState nkl = s_nkl;

    (void)target; // TODO: Use target triple
    NklCompiler c = nk_arena_allocT(&nkl->arena, NklCompiler_T);
    *c = (NklCompiler_T){
        .nkl = nkl,
    };
    return c;
}

NklCompiler nkl_newCompilerHost(void) {
    NK_LOG_TRC("%s", __func__);

    // TODO: Actually fill host target triple
    return nkl_newCompiler((NklTargetTriple){0});
}

// TODO: Reuse TRY macro
#define TRY(EXPR)      \
    do {               \
        if (!(EXPR)) { \
            return 0;  \
        }              \
    } while (0)

NklModule nkl_newModule(NklCompiler c) {
    NK_LOG_TRC("%s", __func__);

    TRY(c);

    NklState nkl = c->nkl;

    NklModule mod = nk_arena_allocT(&nkl->arena, NklModule_T);
    *mod = (NklModule_T){
        .c = c,
        .ir = {.alloc = nk_arena_getAllocator(&nkl->arena)},
    };
    return mod;
}

bool nkl_linkModule(NklModule dst_mod, NklModule src_mod) {
    NK_LOG_TRC("%s", __func__);

    TRY(dst_mod);

    NklState nkl = dst_mod->c->nkl;

    if (!src_mod) {
        nickl_reportError(nkl, (NklSourceLocation){0}, "src_mod is null");
        return false;
    }

    if (dst_mod->c != src_mod->c) {
        nickl_reportError(nkl, (NklSourceLocation){0}, "mixed modules from different compilers");
    }

    nickl_reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_linkModule` is not implemented");
    return false;
}

bool nkl_linkLibrary(NklModule dst_mod, NkString name, NkString library) {
    NK_LOG_TRC("%s", __func__);

    TRY(dst_mod);

    NklState nkl = dst_mod->c->nkl;

    (void)name;
    (void)library;
    nickl_reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_linkLibrary` is not implemented");
    return false;
}

bool nkl_compileFile(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->c->nkl;

    NkString const ext = nk_path_getExtension(file);

    if (nks_equal(ext, nk_cs2s("nkir"))) {
        return nkl_compileFileIr(mod, file);
    } else if (nks_equal(ext, nk_cs2s("nkst"))) {
        return nkl_compileFileAst(mod, file);
    } else if (nks_equal(ext, nk_cs2s("nkl"))) {
        return nkl_compileFileNkl(mod, file);
    } else {
        nickl_reportError(
            nkl,
            (NklSourceLocation){0},
            "Unsupported source file `*." NKS_FMT "`. Supported: `*.nkir`, `*.nkst`, `*.nkl`.",
            NKS_ARG(ext));
        return false;
    }
}

bool nkl_compileFileIr(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->c->nkl;

    // TODO: Canonicalize the file path before atomizing
    NkAtom afile = nk_s2atom(file);

    NklToken err_token = {0};
    NkString err_str = {0};
    if (!nkl_ir_parse(
            &(NklIrParserData){
                .nkl = nkl,
                .file = afile,
                .err_str = &err_str,
                .err_token = &err_token,
                .token_names = s_ir_tokens,
            },
            &mod->ir)) {
        nickl_reportError(
            nkl,
            (NklSourceLocation){
                .file = file,
                .lin = err_token.lin,
                .col = err_token.col,
                .len = err_token.len,
            },
            NKS_FMT,
            NKS_ARG(err_str));
        return false;
    }

    nickl_reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_compileFileIr` is not finished");
    return false;
}

bool nkl_compileFileAst(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->c->nkl;

    // TODO: Canonicalize the file path before atomizing
    NkAtom afile = nk_s2atom(file);

    NklAstNodeArray nodes;
    TRY(nickl_getAst(nkl, afile, &nodes));

    nickl_reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_compileFileAst` is not finished");
    return false;
}

bool nkl_compileFileNkl(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->c->nkl;

    (void)file;
    nickl_reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_compileFileNkl` is not implemented");
    return false;
}

bool nkl_exportModule(NklModule mod, NkString out_file, NklOutputKind kind) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->c->nkl;

    (void)out_file;
    (void)kind;
    nickl_reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_exportModule` is not implemented");
    return false;
}

NklError const *nkl_getErrors(NklState nkl) {
    return nkl->error;
}
