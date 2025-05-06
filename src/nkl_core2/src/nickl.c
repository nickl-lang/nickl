#include "nkl/core/nickl.h"

#include "nkl/core/lexer.h"
#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/error.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

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

static i32 vreportError(NklState nkl, char const *fmt, va_list ap) {
    NklError *err = nk_arena_allocT(&nkl->arena, NklError);
    *err = (NklError){
        .next = nkl->error,
        .msg = {0},
        .file = 0,
        .token = NULL,
    };
    nkl->error = err;

    NkStringBuilder sb = (NkStringBuilder){NKSB_INIT(nk_arena_getAllocator(&nkl->arena))};
    i32 res = nksb_vprintf(&sb, fmt, ap);
    nk_arena_pop(&nkl->arena, sb.capacity - sb.size);
    err->msg = (NkString){NKS_INIT(sb)};

    return res;
}

NK_PRINTF_LIKE(2) static int reportError(NklState nkl, char const *fmt, ...) {
    NK_LOG_TRC("%s", __func__);

    va_list ap;
    va_start(ap, fmt);
    i32 res = vreportError(nkl, fmt, ap);
    va_end(ap);

    return res;
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
        reportError(nkl, "src_mod is null");
        return false;
    }

    if (dst_mod->c != src_mod->c) {
        reportError(nkl, "mixed modules from different compilers");
    }

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
            nkl, "Unsupported source file `*." NKS_FMT "`. Supported: `*.nkir`, `*.nkst`, `*.nkl`.", NKS_ARG(ext));
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
        reportError(nkl, "failed to read file `" NKS_FMT "`: %s", NKS_ARG(file), nk_getLastErrorString());
        return false;
    }

    NklTokenArray tokens = nkl_lex(
        &(NklLexerData){
            .keywords = s_ir_keywords,
            .operators = s_ir_operators,
            .tag_prefixes = s_ir_tag_prefixes,
            .keywords_base = NklIrToken_KeywordsBase,
            .operators_base = NklIrToken_OperatorsBase,
            .tags_base = NklIrToken_TagsBase,
        },
        &nkl->arena,
        text);

    reportError(nkl, "TODO: `nkl_compileFileIr` is not finished");
    return false;
}

bool nkl_compileFileAst(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    (void)file;
    reportError(nkl, "TODO: `nkl_compileFileAst` is not implemented");
    return false;
}

bool nkl_compileFileNkl(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    (void)file;
    reportError(nkl, "TODO: `nkl_compileFileNkl` is not implemented");
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
    reportError(nkl, "TODO: `nkl_exportModule` is not implemented");
    return false;
}

NklError const *nkl_getErrors(NklState nkl) {
    return nkl->error;
}
