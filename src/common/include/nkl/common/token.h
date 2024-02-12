#ifndef NKL_COMMON_TOKEN_H_
#define NKL_COMMON_TOKEN_H_

#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    u32 id;
    u32 pos;
    u32 len;
    u32 lin;
    u32 col;
} NklToken;

typedef NkDynArray(NklToken) NklTokenDynArray;
typedef NkSlice(NklToken const) NklTokenArray;

inline NkString nkl_getTokenStr(NklToken token, NkString text) {
    return NK_LITERAL(NkString){text.data + token.pos, token.len};
}

#ifdef __cplusplus
}
#endif

#endif // NKL_COMMON_TOKEN_H_
