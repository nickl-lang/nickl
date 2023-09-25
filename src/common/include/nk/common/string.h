#ifndef HEADER_GUARD_NK_COMMON_STRING_H
#define HEADER_GUARD_NK_COMMON_STRING_H

#include <stddef.h>
#include <string.h>

#include "nk/common/allocator.h"
#include "nk/sys/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char const *data;
    size_t size;
} nkstr;

NK_INLINE nkstr nk_mkstr(char const *str) {
    return {str, strlen(str)};
}

NK_INLINE nkstr nk_strcpy(NkAllocator alloc, nkstr src) {
    auto mem = nk_alloc(alloc, src.size);
    memcpy(mem, src.data, src.size);
    return {(char *)mem, src.size};
}

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_STRING_H
