#ifndef NKL_CORE_NICKL_H_
#define NKL_CORE_NICKL_H_

#include <stdarg.h>

#include "nkl/common/token.h"
#include "ntk/arena.h"
#include "ntk/hash_tree.h"
#include "ntk/os/common.h"
#include "ntk/slice.h"
#include "ntk/string.h"

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
NK_HASH_TREE_TYPEDEF(TypeTree, Type_kv);
NK_HASH_TREE_PROTO(TypeTree, Type_kv, ByteArray);

typedef struct {
    NkArena type_arena;
    TypeTree type_tree;
    NkOsHandle mtx;
    u32 next_id;

    // TODO: Use scratch arenas
    NkArena tmp_arena;

} NklTypeStorage;

typedef struct {
    NklTypeStorage types;
} NklState;

void nkl_state_init(NklState *nkl);
void nkl_state_free(void);

typedef struct NklError {
    struct NklError *next;
    NkString msg;
    NklToken const *token;
} NklError;

typedef struct NklErrorState {
    NkArena *arena;
    NklError *errors;
    NklError *last_error;
    usize error_count;
} NklErrorState;

void nkl_errorStateInitAndEquip(NklErrorState *state, NkArena *arena);

usize nkl_getErrorCount(void);

NK_PRINTF_LIKE(2, 3) i32 nkl_reportError(NklToken const *token, char const *fmt, ...);
i32 nkl_vreportError(NklToken const *token, char const *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_NICKL_H_
