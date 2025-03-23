#include "ntk/error.h"

#include "errno.h"
#include "string.h"

nkerr_t nk_getLastError(void) {
    return errno;
}

void nk_setLastError(nkerr_t err) {
    errno = err;
}

char const *nk_getErrorString(nkerr_t err) {
    return strerror(err);
}
