#ifndef HEADER_GUARD_NKL_LANG_TOKEN
#define HEADER_GUARD_NKL_LANG_TOKEN

#include <stddef.h>

#include "nk/common/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nkstr text;

    size_t pos;
    size_t lin;
    size_t col;

    size_t id;
} NklToken; // TODO Move NklToken declaration

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_TOKEN
