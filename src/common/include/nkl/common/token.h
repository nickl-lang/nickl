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
    uint32_t id;
    uint32_t pos;
    uint32_t len;
    uint32_t lin;
    uint32_t col;
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
