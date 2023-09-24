#include "nk/sys/error.h"

#include "errno.h"
#include "string.h"

nkerr_t nk_getLastError(void) {
    return errno;
}

char const *nk_getErrorString(nkerr_t err) {
    return strerror(err);
}

extern inline char const *nk_getLastErrorString(void);
