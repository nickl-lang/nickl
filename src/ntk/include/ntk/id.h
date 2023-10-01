#ifndef HEADER_GUARD_NTK_ID
#define HEADER_GUARD_NTK_ID

#include <stddef.h>
#include <stdint.h>

#include "ntk/string.h"

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

#endif // HEADER_GUARD_NTK_ID
