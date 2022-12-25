#include "nk/common/string_builder.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

#include "nk/common/allocator.h"

typedef struct NkStringBuilder_T {
    NkAllocator *alloc;
    char *data;
    size_t size;
} NkStringBuilder_T;

NkStringBuilder nksb_create() {
    return nksb_create_alloc(nk_default_allocator);
}

NkStringBuilder nksb_create_alloc(NkAllocator *alloc) {
    NkStringBuilder sb = new (nk_allocate(alloc, sizeof(*sb))) NkStringBuilder_T{
        .alloc = alloc,
        .data = (char *)nk_allocate(alloc, 1),
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
    int const printf_res = std::vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    size_t const new_size = sb->size + printf_res;

    char *new_data = (char *)nk_allocate(sb->alloc, new_size + 1);
    std::memcpy(new_data, sb->data, sb->size);

    va_copy(ap_copy, ap);
    std::vsnprintf(new_data + sb->size, printf_res + 1, fmt, ap_copy);
    va_end(ap_copy);

    new_data[new_size] = 0;

    nk_free(sb->alloc, sb->data);

    sb->data = new_data;
    sb->size = new_size;

    return printf_res;
}

nkstr nksb_concat(NkStringBuilder sb) {
    return {sb->data, sb->size};
}

void nksb_str_escape(NkStringBuilder sb, nkstr str) {
    for (size_t i = 0; i < str.size; i++) {
        switch (str.data[i]) {
        case '\a':
            nksb_printf(sb, "\\a");
            break;
        case '\b':
            nksb_printf(sb, "\\b");
            break;
        case '\f':
            nksb_printf(sb, "\\f");
            break;
        case '\n':
            nksb_printf(sb, "\\n");
            break;
        case '\r':
            nksb_printf(sb, "\\r");
            break;
        case '\t':
            nksb_printf(sb, "\\t");
            break;
        case '\v':
            nksb_printf(sb, "\\v");
            break;
        case '\0':
            nksb_printf(sb, "\\0");
            break;
        case '\'':
            nksb_printf(sb, "\\'");
            break;
        case '\\':
            nksb_printf(sb, "\\\\");
            break;
        default:
            nksb_printf(sb, "%c", str.data[i]);
            break;
        }
    }
}