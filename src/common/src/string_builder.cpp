#include "nk/common/string_builder.h"

#include <cctype>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

#include "nk/common/array.hpp"

NkStringBuilder_T *nksb_create() {
    return nksb_create_alloc(nk_default_allocator);
}

NkStringBuilder_T *nksb_create_alloc(NkAllocator alloc) {
    NkStringBuilder_T *sb = new (nk_alloc(alloc, sizeof(*sb))) NkStringBuilder_T{};
    nksb_init_alloc(sb, alloc);
    return sb;
}

void nksb_init(NkStringBuilder_T *sb) {
    nksb_init_alloc(sb, nk_default_allocator);
}

void nksb_init_alloc(NkStringBuilder_T *sb, NkAllocator alloc) {
    *sb = {
        .data = {},
        .size = {},
        .capacity = {},
        .alloc = alloc,
    };
}

void nksb_deinit(NkStringBuilder_T *sb) {
    auto &ar = (NkArray<char> &)(*sb);
    ar.deinit();
}

void nksb_free(NkStringBuilder_T *sb) {
    if (sb) {
        nksb_deinit(sb);
        nk_free(sb->alloc, sb, sizeof(*sb));
    }
}

int nksb_printf(NkStringBuilder_T *sb, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = nksb_vprintf(sb, fmt, ap);
    va_end(ap);

    return res;
}

int nksb_vprintf(NkStringBuilder_T *sb, char const *fmt, va_list ap) {
    auto &ar = (NkArray<char> &)(*sb);

    if (!ar.capacity()) {
        ar.reserve(1000);
    }

    va_list ap_copy;

    va_copy(ap_copy, ap);
    int const printf_res = vsnprintf(nullptr, 0, fmt, ap_copy);
    va_end(ap_copy);

    size_t const alloc_size = printf_res + 1;
    auto const slice = ar.push(alloc_size);
    ar.pop(1);

    va_copy(ap_copy, ap);
    vsnprintf(slice.data(), alloc_size, fmt, ap_copy);
    va_end(ap_copy);

    return printf_res;
}

nkstr nksb_concat(NkStringBuilder_T *sb) {
    return {sb->data, sb->size};
}

void nksb_str_escape(NkStringBuilder_T *sb, nkstr str) {
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
        case '\"':
            nksb_printf(sb, "\\\"");
            break;
        case '\\':
            nksb_printf(sb, "\\\\");
            break;
        default:
            if (isprint(str.data[i])) {
                nksb_printf(sb, "%c", str.data[i]);
            } else {
                nksb_printf(sb, "\\x%" PRIx8, str.data[i] & 0xff);
            }
            break;
        }
    }
}

void nksb_str_unescape(NkStringBuilder_T *sb, nkstr str) {
    for (size_t i = 0; i < str.size; i++) {
        if (str.data[i] == '\\' && i < str.size - 1) {
            switch (str.data[++i]) {
            case 'a':
                nksb_printf(sb, "%c", '\a');
                break;
            case 'b':
                nksb_printf(sb, "%c", '\b');
                break;
            case 'f':
                nksb_printf(sb, "%c", '\f');
                break;
            case 'n':
                nksb_printf(sb, "%c", '\n');
                break;
            case 'r':
                nksb_printf(sb, "%c", '\r');
                break;
            case 't':
                nksb_printf(sb, "%c", '\t');
                break;
            case 'v':
                nksb_printf(sb, "%c", '\v');
                break;
            case '0':
                nksb_printf(sb, "%c", '\0');
                break;
            default:
                nksb_printf(sb, "%c", str.data[i]);
                break;
            }
        } else {
            nksb_printf(sb, "%c", str.data[i]);
        }
    }
}
