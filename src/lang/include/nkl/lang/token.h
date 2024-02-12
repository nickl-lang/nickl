#ifndef NKL_LANG_TOKEN_H_
#define NKL_LANG_TOKEN_H_

#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkString text;

    usize pos;
    usize lin;
    usize col;

    usize id;
} NklToken;

typedef NklToken const *NklTokenRef;

typedef NkSlice(NklToken const) NklTokenArray;

#ifdef __cplusplus
}
#endif

#endif // NKL_LANG_TOKEN_H_
