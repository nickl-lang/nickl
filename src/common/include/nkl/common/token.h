#ifndef HEADER_GUARD_NKL_COMMON_TOKEN
#define HEADER_GUARD_NKL_COMMON_TOKEN

#include <stdint.h>

#include "ntk/array.h"
#include "ntk/common.h"
#include "ntk/id.h"
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

nkar_typedef(NklToken, NklTokenArray);
nkav_typedef(NklToken const, NklTokenView);

inline nks nkl_getTokenStr(NklToken token, nks text) {
    return LITERAL(nks){text.data + token.pos, token.len};
}

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_COMMON_TOKEN
