#ifndef HEADER_GUARD_NK_SYS_ERROR
#define HEADER_GUARD_NK_SYS_ERROR

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t nkerr_t;

nkerr_t nk_getLastError(void);
char const *nk_getErrorString(nkerr_t err);

NK_INLINE char const *nk_getLastErrorString(void) {
    return nk_getErrorString(nk_getLastError());
}

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_SYS_ERROR
