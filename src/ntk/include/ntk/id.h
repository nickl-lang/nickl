#ifndef HEADER_GUARD_NTK_ID
#define HEADER_GUARD_NTK_ID

#include <stddef.h>
#include <stdint.h>

#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u32 nkid;

nks nkid2s(nkid id);
char const *nkid2cs(nkid id);

nkid s2nkid(nks str);
nkid cs2nkid(char const *str);

void nkid_define(nkid id, nks str);

enum {
    nk_invalid_id = 0,
};

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_ID
