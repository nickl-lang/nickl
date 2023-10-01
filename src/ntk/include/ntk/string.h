#ifndef HEADER_GUARD_NTK_STRING_H
#define HEADER_GUARD_NTK_STRING_H

#include <stddef.h>
#include <string.h>

#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

nkav_typedef(char const, nks);

NK_INLINE nks nk_cs2s(char const *str) {
    return LITERAL(nks){str, strlen(str)};
}

NK_INLINE nks nk_strcpy(NkAllocator alloc, nks src) {
    void *mem = nk_alloc(alloc, src.size);
    memcpy(mem, src.data, src.size);
    return LITERAL(nks){(char const *)mem, src.size};
}

NK_INLINE nks nks_trim_left(nks str) {
    while (str.size && nkav_first(str) == ' ') {
        str.size -= 1;
        str.data += 1;
    }
    return str;
}

NK_INLINE nks nks_trim_right(nks str) {
    while (str.size && nkav_last(str) == ' ') {
        str.size -= 1;
    }
    return str;
}

NK_INLINE nks nks_trim(nks str) {
    return nks_trim_right(nks_trim_left(str));
}

NK_INLINE nks nks_chop_by_delim(nks *str, char delim) {
    size_t i = 0;
    while (i < str->size && str->data[i] != delim) {
        i += 1;
    }

    nks res = {str->data, i};

    if (i < str->size) {
        str->size -= i + 1;
        str->data += i + 1;
    } else {
        str->size -= i;
        str->data += i;
    }

    return res;
}

#define nks_Fmt "%.*s"
#define nks_Arg(str) (int)(str).size, (str).data

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_STRING_H
