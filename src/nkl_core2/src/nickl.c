#include "nkl/core/nickl.h"

#include "nkl/common/diagnostics.h"
#include "nkl/common/token.h"
#include "nkl/core/lexer.h"
#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/error.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/slice.h"
#include "ntk/string.h"

NK_LOG_USE_SCOPE(nickl);

typedef struct NklState_T {
    struct NklState_T *next;

    NkArena arena;
    NklError *error;
} NklState_T;

typedef struct NklCompiler_T {
    NklState nkl;
} NklCompiler_T;

typedef struct NklModule_T {
    NklCompiler c;
} NklModule_T;

static void vreportError(NklState nkl, NklSourceLocation loc, char const *fmt, va_list ap) {
    NklError *err = nk_arena_allocT(&nkl->arena, NklError);
    *err = (NklError){
        .next = nkl->error,
        .msg = nk_vtsprintf(&nkl->arena, fmt, ap),
        .loc = loc,
    };
    nkl->error = err;
}

NK_PRINTF_LIKE(3) static void reportError(NklState nkl, NklSourceLocation loc, char const *fmt, ...) {
    NK_LOG_TRC("%s", __func__);

    va_list ap;
    va_start(ap, fmt);
    vreportError(nkl, loc, fmt, ap);
    va_end(ap);
}

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

static _Thread_local NklState s_nkl;

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

NklModule nkl_newModule(NklCompiler c) {
    NK_LOG_TRC("%s", __func__);

    if (!c) {
        return NULL;
    }

    NklState nkl = c->nkl;

    NklModule mod = nk_arena_allocT(&nkl->arena, NklModule_T);
    *mod = (NklModule_T){
        .c = c,
    };
    return mod;
}

bool nkl_linkModule(NklModule dst_mod, NklModule src_mod) {
    NK_LOG_TRC("%s", __func__);

    if (!dst_mod) {
        return false;
    }

    NklState nkl = dst_mod->c->nkl;

    if (!src_mod) {
        reportError(nkl, (NklSourceLocation){0}, "src_mod is null");
        return false;
    }

    if (dst_mod->c != src_mod->c) {
        reportError(nkl, (NklSourceLocation){0}, "mixed modules from different compilers");
    }

    reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_linkModule` is not implemented");
    return false;
}

bool nkl_linkLibrary(NklModule dst_mod, NkString name, NkString library) {
    NK_LOG_TRC("%s", __func__);

    if (!dst_mod) {
        return false;
    }

    NklState nkl = dst_mod->c->nkl;

    (void)name;
    (void)library;
    reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_linkLibrary` is not implemented");
    return false;
}

bool nkl_compileFile(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    NkString const ext = nk_path_getExtension(file);

    if (nks_equal(ext, nk_cs2s("nkir"))) {
        return nkl_compileFileIr(mod, file);
    } else if (nks_equal(ext, nk_cs2s("nkst"))) {
        return nkl_compileFileAst(mod, file);
    } else if (nks_equal(ext, nk_cs2s("nkl"))) {
        return nkl_compileFileNkl(mod, file);
    } else {
        reportError(
            nkl,
            (NklSourceLocation){0},
            "Unsupported source file `*." NKS_FMT "`. Supported: `*.nkir`, `*.nkst`, `*.nkl`.",
            NKS_ARG(ext));
        return false;
    }
}

char const *s_ir_keywords[] = {
    "pub",
    "proc",
    "i32",
    "ret",

    NULL,
};

char const *s_ir_operators[] = {
    "(",
    ")",
    "{",
    "}",

    NULL,
};

char const s_ir_tag_prefixes[] = {
    '@',

    0,
};

enum {
    NklIrToken_KeywordsBase = NklBaseToken_Count,

    NklIrToken_Pub,
    NklIrToken_Proc,
    NklIrToken_I32,
    NklIrToken_Ret,

    NklIrToken_OperatorsBase,

    NklIrToken_LParen,
    NklIrToken_RParen,
    NklIrToken_LBrace,
    NklIrToken_RBrace,

    NklIrToken_TagsBase,

    NklIrToken_AtTag,
};

bool nkl_compileFileIr(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    NkString text;
    if (!nk_file_read(nk_arena_getAllocator(&nkl->arena), file, &text)) {
        reportError(
            nkl,
            (NklSourceLocation){0},
            "failed to read file `" NKS_FMT "`: %s",
            NKS_ARG(file),
            nk_getLastErrorString());
        return false;
    }

    NkString lex_error;
    NklTokenArray tokens;
    if (!nkl_lex(
            &(NklLexerData){
                .text = text,
                .arena = &nkl->arena,
                .error = &lex_error,

                .keywords = s_ir_keywords,
                .operators = s_ir_operators,
                .tag_prefixes = s_ir_tag_prefixes,

                .keywords_base = NklIrToken_KeywordsBase,
                .operators_base = NklIrToken_OperatorsBase,
                .tags_base = NklIrToken_TagsBase,
            },
            &tokens)) {
        NklToken err_token = nk_slice_last(tokens);
        reportError(
            nkl,
            (NklSourceLocation){
                .file = file,
                .lin = err_token.lin,
                .col = err_token.col,
                .len = err_token.len,
            },
            NKS_FMT,
            NKS_ARG(lex_error));
        return false;
    }

    reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_compileFileIr` is not finished");
    return false;
}

bool nkl_compileFileAst(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    (void)file;
    reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_compileFileAst` is not implemented");
    return false;
}

bool nkl_compileFileNkl(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    (void)file;
    reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_compileFileNkl` is not implemented");
    return false;
}

bool nkl_exportModule(NklModule mod, NkString out_file, NklOutputKind kind) {
    NK_LOG_TRC("%s", __func__);

    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    (void)out_file;
    (void)kind;
    reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_exportModule` is not implemented");
    return false;
}

NklError const *nkl_getErrors(NklState nkl) {
    return nkl->error;
}
