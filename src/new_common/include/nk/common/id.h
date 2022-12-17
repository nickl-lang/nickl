#ifndef HEADER_GUARD_NK_COMMON_ID
#define HEADER_GUARD_NK_COMMON_ID

#include <stddef.h>
#include <stdint.h>

#include "nk/common/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t Id;

nkstr nkid2s(Id id);
char const *nkid2cs(Id id);

Id s2nkid(nkstr str);
Id cs2nkid(char const *str);

void nkid_define(Id id, nkstr str);

enum {
    id_empty = 0,
};

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_ID
