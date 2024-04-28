#ifndef NTK_ERROR_H_
#define NTK_ERROR_H_

#include <stdarg.h>

#include "ntk/allocator.h"
#include "ntk/common.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkErrorNode {
    struct NkErrorNode *next;
    NkString msg;
} NkErrorNode;

typedef struct NkErrorState {
    NkAllocator alloc;

    struct NkErrorState *next;

    NkErrorNode *errors;
    NkErrorNode *last_error;
    usize error_count;
} NkErrorState;

void nk_error_pushState(NkErrorState *state);
void nk_error_popState(void);

void nk_error_freeState(void);

NK_PRINTF_LIKE(1, 2) i32 nk_error_printf(char const *fmt, ...);
i32 nk_error_vprintf(char const *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif // NTK_ERROR_H_
