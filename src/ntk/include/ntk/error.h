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

NK_EXPORT void nk_error_pushState(NkErrorState *state);
NK_EXPORT void nk_error_popState(void);

NK_EXPORT void nk_error_freeState(void);

NK_EXPORT usize nk_error_count(void);

// TODO: Add location info
NK_EXPORT NK_PRINTF_LIKE(1) i32 nk_error_printf(char const *fmt, ...);
NK_EXPORT i32 nk_error_vprintf(char const *fmt, va_list ap);

typedef u32 nkerr_t;

NK_EXPORT nkerr_t nk_getLastError(void);
NK_EXPORT void nk_setLastError(nkerr_t err);

NK_EXPORT char const *nk_getErrorString(nkerr_t err);

NK_INLINE char const *nk_getLastErrorString(void) {
    return nk_getErrorString(nk_getLastError());
}

#ifdef __cplusplus
}
#endif

#endif // NTK_ERROR_H_
