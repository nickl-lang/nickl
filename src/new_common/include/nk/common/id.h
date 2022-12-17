#ifndef HEADER_GUARD_NK_COMMON_ID
#define HEADER_GUARD_NK_COMMON_ID

#include <stddef.h>
#include <stdint.h>

#include "nk/common/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t Id;

string nkid2s(Id id);
char const *nkid2cs(Id id);

Id s2nkid(string str);
Id cs2nkid(char const *str);

void nkid_define(Id id, string str);

enum {
    id_empty = 0,
};

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_ID
