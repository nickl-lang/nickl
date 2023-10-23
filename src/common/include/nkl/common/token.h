#ifndef HEADER_GUARD_NKL_COMMON_TOKEN
#define HEADER_GUARD_NKL_COMMON_TOKEN

#include <stdint.h>

#include "ntk/array.h"
#include "ntk/string.h"

#ifndef __cplusplus
extern "C" {
#endif

typedef struct {
    nks text;
    uint32_t id;
    uint32_t lin;
    uint32_t col;
} NklToken;

nkar_typedef(NklToken, NklTokenArray);
nkav_typedef(NklToken, NklTokenView);

#ifndef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_COMMON_TOKEN
