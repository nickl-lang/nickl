#ifndef NKL_CORE_NICKL_IMPL_H_
#define NKL_CORE_NICKL_IMPL_H_

#include "nkb/ir.h"
#include "nkl/common/ast.h"
#include "nkl/common/diagnostics.h"
#include "nkl/common/token.h"
#include "nkl/core/nickl.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/hash_tree.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NklState_T {
    NkArena arena;
    NkArena scratch;

    NkbState nkb;

    // TODO: Add type safe wrapper for the hash tree
    NkIntptrHashTree text_map;

    NklError *error;
} NklState_T;

typedef struct NklCompiler_T {
    NklState nkl;
    NkIrTarget target;

    // TODO: Add type safe wrapper for the hash tree
    NkIntptrHashTree lib_aliases;

    usize module_count;
} NklCompiler_T;

typedef NkDynArray(NklModule) NklModuleDynArray;

typedef struct NklModule_T {
    NkAtom name;

    NklCompiler com;
    NkIrModule ir;

    // TODO: Add type safe wrapper for the hash tree
    NkIntptrHashTree linked_mods;
    NkIntptrHashTree extern_syms;

    NklModuleDynArray mods_linked_to;
} NklModule_T;

extern char const *s_ir_tokens[];
extern char const *s_ast_tokens[];

NK_PRINTF_LIKE(2) void nickl_reportError(NklState nkl, char const *fmt, ...);
NK_PRINTF_LIKE(3) void nickl_reportErrorLoc(NklState nkl, NklSourceLocation loc, char const *fmt, ...);
void nickl_vreportError(NklState nkl, NklSourceLocation loc, char const *fmt, va_list ap);

bool nickl_getText(NklState nkl, NkAtom file, NkString *out_text);
bool nickl_defineText(NklState nkl, NkAtom file, NkString text);

bool nickl_getTokensIr(NklState nkl, NkAtom file, NklTokenArray *out_tokens);
bool nickl_getTokensAst(NklState nkl, NkAtom file, NklTokenArray *out_tokens);

bool nickl_getAst(NklState nkl, NkAtom file, NklAstNodeArray *out_nodes);

NkAtom nickl_canonicalizePath(NkString base, NkString path);
NkAtom nickl_findFile(NklState nkl, NkAtom base, NkString name);

NkAtom nickl_translateLib(NklCompiler com, NkAtom alias);

typedef struct {
    NkAtom name;
    NkIrVisibility vis;
    NkIrSymbolFlags flags;
} NklSymbolInfo;

bool nickl_defineProc(NklModule mod, NkIrProc const *proc, NklSymbolInfo const *info);
bool nickl_defineData(NklModule mod, NkIrData const *data, NklSymbolInfo const *info);
bool nickl_defineExtern(NklModule mod, NkIrExtern const *extrn, NklSymbolInfo const *info, NkAtom lib);

bool nickl_linkSymbol(NklModule dst_mod, NklModule src_mod, NkIrSymbol const *sym);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_NICKL_IMPL_H_
