#ifndef HEADER_GUARD_NKL_LANG_TOKEN
#define HEADER_GUARD_NKL_LANG_TOKEN

#include <stddef.h>

#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nks text;

    usize pos;
    usize lin;
    usize col;

    usize id;
} NklToken;

typedef NklToken const *NklTokenRef;

nkav_typedef(NklToken const, NklTokenView);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_TOKEN
