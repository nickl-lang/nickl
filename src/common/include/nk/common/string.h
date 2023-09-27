#ifndef HEADER_GUARD_NK_COMMON_STRING_H
#define HEADER_GUARD_NK_COMMON_STRING_H

#include <stddef.h>
#include <string.h>

#include "nk/common/allocator.h"
#include "nk/common/array.h"
#include "nk/sys/common.h"

#ifdef __cplusplus
extern "C" {
#endif

nkav_typedef(char const, nkstr);

NK_INLINE nkstr nk_mkstr(char const *str) {
    return LITERAL(nkstr){str, strlen(str)};
}

NK_INLINE nkstr nk_strcpy(NkAllocator alloc, nkstr src) {
    void *mem = nk_alloc(alloc, src.size);
    memcpy(mem, src.data, src.size);
    return LITERAL(nkstr){(char const *)mem, src.size};
}

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_STRING_H
