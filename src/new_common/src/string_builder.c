#include "nk/common/string_builder.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct NkStringBuilder_T {
    NkAllocator *alloc;
    char *data;
    size_t size;
} NkStringBuilder_T;

NkStringBuilder nksb_create() {
    return nksb_create_alloc(nk_default_allocator);
}

NkStringBuilder nksb_create_alloc(NkAllocator *alloc) {
    NkStringBuilder sb = (NkStringBuilder)nk_allocate(alloc, sizeof(*sb));
    *sb = (NkStringBuilder_T){
        .alloc = alloc,
        .data = nk_allocate(alloc, 1),
        .size = 0,
    };
    sb->data[0] = 0;
    return sb;
}

void nksb_free(NkStringBuilder sb) {
    nk_free(sb->alloc, sb->data);
    nk_free(sb->alloc, sb);
}

int nksb_printf(NkStringBuilder sb, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = nksb_vprintf(sb, fmt, ap);
    va_end(ap);

    return res;
}

// TODO Performance. Incredibly inefficient nksb_vprintf
int nksb_vprintf(NkStringBuilder sb, char const *fmt, va_list ap) {
    va_list ap_copy;

    va_copy(ap_copy, ap);
    int const printf_res = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    size_t const new_size = sb->size + printf_res;

    char *new_data = (char *)nk_allocate(sb->alloc, new_size + 1);
    memcpy(new_data, sb->data, sb->size);

    va_copy(ap_copy, ap);
    vsnprintf(new_data + sb->size, printf_res + 1, fmt, ap_copy);
    va_end(ap_copy);

    new_data[new_size] = 0;

    nk_free(sb->alloc, sb->data);

    sb->data = new_data;
    sb->size = new_size;

    return printf_res;
}

char const *nksb_concat(NkStringBuilder sb) {
    return sb->data;
}
