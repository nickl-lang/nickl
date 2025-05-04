#include "nkl/core/nickl.h"

#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/string_builder.h"

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
    va_list ap;
    va_start(ap, fmt);
    i32 res = vreportError(nkl, fmt, ap);
    va_end(ap);

    return res;
}

NklState nkl_newState() {
    NkArena arena = {0};
    NklState nkl = nk_arena_allocT(&arena, NklState_T);
    *nkl = (NklState_T){
        .arena = arena,
        .error = NULL,
    };
    return nkl;
}

void nkl_freeState(NklState nkl) {
    NkArena arena = nkl->arena;
    nk_arena_free(&arena);
}

static _Thread_local NklState s_nkl;

void nkl_pushState(NklState nkl) {
    nkl->next = s_nkl;
    s_nkl = nkl;
}

void nkl_popState() {
    nk_assert(s_nkl && "no active state");
    s_nkl = s_nkl->next;
}

NklCompiler nkl_newCompiler(NklTargetTriple target) {
    nk_assert(s_nkl && "no active state");
    NklState nkl = s_nkl;

    reportError(nkl, "TODO: `nkl_newCompiler` is not implemented");
    return NULL;
}

NklCompiler nkl_newCompilerHost() {
    // TODO: Actually fill host target tripple
    return nkl_newCompiler((NklTargetTriple){0});
}

NklModule nkl_newModule(NklCompiler c) {
    if (!c) {
        return NULL;
    }

    NklState nkl = c->nkl;

    reportError(nkl, "TODO: `nkl_newCompiler` is not implemented");
    return NULL;
}

bool nkl_linkModule(NklModule dst_mod, NklModule src_mod) {
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

bool nkl_compileFile(NklModule mod, NkString in_file) {
    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    reportError(nkl, "TODO: `nkl_compileFile` is not implemented");
    return false;
}

bool nkl_compileFileIr(NklModule mod, NkString in_file) {
    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    reportError(nkl, "TODO: `nkl_compileFileIr` is not implemented");
    return false;
}

bool nkl_compileFileAst(NklModule mod, NkString in_file) {
    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    reportError(nkl, "TODO: `nkl_compileFileAst` is not implemented");
    return false;
}

bool nkl_compileFileNkl(NklModule mod, NkString in_file) {
    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    reportError(nkl, "TODO: `nkl_compileFileNkl` is not implemented");
    return false;
}

bool nkl_exportModule(NklModule mod, NkString out_file, NklOutputKind kind) {
    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    reportError(nkl, "TODO: `nkl_exportModule` is not implemented");
    return false;
}

NklError const *nkl_getErrors(NklState nkl) {
    return nkl->error;
}
