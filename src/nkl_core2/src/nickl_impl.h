#ifndef NKL_CORE_NICKL_IMPL_H_
#define NKL_CORE_NICKL_IMPL_H_

#include "hash_trees.h"
#include "nkb/ir.h"
#include "nkl/common/ast.h"
#include "nkl/common/diagnostics.h"
#include "nkl/common/token.h"
#include "nkl/core/nickl.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/string.h"
#include "types_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NklState_T {
    NkArena arena;
    NkScratchPair scratch_pair;

    NkbState nkb;
    NkDynArray(NkIrTarget) created_targets;
    NkIrRuntime _rt;

    NkAtomStringMap text_map;

    NklError *error;

    NklTypeStorage types;

#define X(TYPE, VALUE_TYPE) NklType NK_CAT(_cached_, NK_CAT(TYPE, _t));
    NKIR_NUMERIC_ITERATE(X)
#undef X
    NklType _cached_void_t;
    NklType _cached_bool_t;
} NklState_T;

#define CACHED_TYPE(NAME, EXPR)                                \
    NK_INLINE NklType NK_CAT(nickl_get_, NAME)(NklState nkl) { \
        NklType *cached = &nkl->NK_CAT(_cached_, NAME);        \
        if (!*cached) {                                        \
            *cached = (EXPR);                                  \
        }                                                      \
        return *cached;                                        \
    };

#define X(TYPE, VALUE_TYPE) CACHED_TYPE(NK_CAT(TYPE, _t), nkl_type_getNumeric(nkl, VALUE_TYPE))
NKIR_NUMERIC_ITERATE(X)
#undef X

CACHED_TYPE(void_t, nkl_type_getVoid(nkl));
CACHED_TYPE(bool_t, nkl_type_getBool(nkl));

#undef CACHED_TYPE

typedef struct NklCompiler_T {
    NklState nkl;
    NkIrTarget target;

    NkAtomMap lib_aliases;

    usize word_size;
} NklCompiler_T;

typedef NkDynArray(NklModule) NklModuleDynArray;

typedef struct NklModule_T {
    NkAtom name;

    NklCompiler com;
    NkIrSymbolDynArray ir;
    NkIrDylib _dl;

    NkAtomModuleMap linked_mods;
    NkAtomMap extern_syms;

    NklModuleDynArray mods_linked_to;
} NklModule_T;

extern char const *s_ir_tokens[];
extern char const *s_ast_tokens[];

NK_PRINTF_LIKE(2) void nickl_reportError(NklState nkl, char const *fmt, ...);
NK_PRINTF_LIKE(3) void nickl_reportErrorLoc(NklState nkl, NklSourceLocation loc, char const *fmt, ...);
void nickl_vreportError(NklState nkl, NklSourceLocation loc, char const *fmt, va_list ap);

void nickl_printModuleName(NkStream out, NkAtom name);
void nickl_printSymbol(NkStream out, NkAtom mod, NkAtom sym);

bool nickl_getText(NklState nkl, NkAtom file, NkString *out_text);
bool nickl_defineText(NklState nkl, NkAtom file, NkString text);

bool nickl_getTokensIr(NklState nkl, NkAtom file, NklTokenArray *out_tokens);
bool nickl_getTokensAst(NklState nkl, NkAtom file, NklTokenArray *out_tokens);

bool nickl_getAst(NklState nkl, NkAtom file, NklAstNodeArray *out_nodes);

NkAtom nickl_canonicalizePath(NkString base, NkString path);
NkAtom nickl_findFile(NklState nkl, NkAtom base, NkString name);

NkAtom nickl_translateLib(NklCompiler com, NkAtom alias);

bool nickl_defineSymbol(NklModule mod, NkIrSymbol const *sym);

bool nickl_linkSymbol(NklModule dst_mod, NklModule src_mod, NkIrSymbol const *sym);

bool nickl_compile(NklModule mod, NklSource const *src);
bool nickl_TMP_compileAndRunFile(NklModule mod, NklSource const *src);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_NICKL_IMPL_H_
