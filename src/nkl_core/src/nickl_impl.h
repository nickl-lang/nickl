#ifndef NKL_CORE_NICKL_IMPL_H_
#define NKL_CORE_NICKL_IMPL_H_

#include "nkl/core/nickl.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/hash_tree.h"
#include "ntk/os/common.h"
#include "ntk/slice.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NklType_T NklType;

typedef NkSlice(u8) ByteArray;
typedef struct {
    ByteArray key;
    NklType *val;
} Type_kv;
// TODO: Use hash map for types
NK_HASH_TREE_TYPEDEF(TypeMap, Type_kv);
NK_HASH_TREE_PROTO(TypeMap, Type_kv, ByteArray);

typedef struct {
    NkArena type_arena;
    TypeMap type_map;
    NkOsHandle mtx;
    u32 next_id;

    // TODO: Use scratch arenas
    NkArena tmp_arena;
} NklTypeStorage;

typedef struct {
    NkAtom key;
    NklSource val;
} Source_kv;
NK_HASH_TREE_TYPEDEF(FileMap, Source_kv);
NK_HASH_TREE_PROTO(FileMap, Source_kv, NkAtom);

typedef struct NklState_T {
    NkArena permanent_arena;

    NklTypeStorage types;

    NklLexerProc lexer_proc;
    NklParserProc parser_proc;

    FileMap files;

    StringSlice cli_args;
} NklState_T;

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_NICKL_IMPL_H_
