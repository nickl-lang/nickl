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

NK_INLINE nkstr nks_trim_left(nkstr str) {
    while (str.size && nkav_first(str) == ' ') {
        str.size -= 1;
        str.data += 1;
    }
    return str;
}

NK_INLINE nkstr nks_trim_right(nkstr str) {
    while (str.size && nkav_last(str) == ' ') {
        str.size -= 1;
    }
    return str;
}

NK_INLINE nkstr nks_trim(nkstr str) {
    return nks_trim_right(nks_trim_left(str));
}

NK_INLINE nkstr nks_chop_by_delim(nkstr *str, char delim) {
    size_t i = 0;
    while (i < str->size && str->data[i] != delim) {
        i += 1;
    }

    nkstr res = {str->data, i};

    if (i < str->size) {
        str->size -= i + 1;
        str->data += i + 1;
    } else {
        str->size -= i;
        str->data += i;
    }

    return res;
}

#define nkstr_Fmt "%.*s"
#define nkstr_Arg(str) (int)(str).size, (str).data

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_STRING_H
