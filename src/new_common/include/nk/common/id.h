#ifndef HEADER_GUARD_NK_COMMON_ID
#define HEADER_GUARD_NK_COMMON_ID

#include <stddef.h>
#include <stdint.h>

#include "nk/common/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t nkid;

nkstr nkid2s(nkid id);
char const *nkid2cs(nkid id);

nkid s2nkid(nkstr str);
nkid cs2nkid(char const *str);

void nkid_define(nkid id, nkstr str);

enum {
    nkid_empty = 0,
};

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_ID
