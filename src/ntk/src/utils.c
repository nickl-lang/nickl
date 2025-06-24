#include "ntk/utils.h"

#include <stdlib.h>

#include "ntk/stream.h"
#include "ntk/string_builder.h"

#define strtof32 strtof
#define strtof64 strtod

#define PRINT_EXACT_FLOAT(BITS)                                             \
    do {                                                                    \
        NKSB_FIXED_BUFFER(buf, 50);                                         \
        NkStream const out_buf = nksb_getStream(&buf);                      \
                                                                            \
        nk_printf(out_buf, "%g", val);                                      \
        nksb_appendNull(&buf);                                              \
        char const *cstr = buf.data;                                        \
                                                                            \
        char *endptr = NULL;                                                \
        NK_CAT(f, BITS) const actual = NK_CAT(strtof, BITS)(cstr, &endptr); \
        nk_assert(endptr == cstr + buf.size - 1);                           \
                                                                            \
        if (val == actual) {                                                \
            nk_printf(out, "%s", cstr);                                     \
                                                                            \
            char const *p = cstr;                                           \
            for (; *p; p++) {                                               \
                if (*p == '.') {                                            \
                    break;                                                  \
                }                                                           \
            }                                                               \
            if (!*p) {                                                      \
                nk_printf(out, ".0");                                       \
            }                                                               \
        } else {                                                            \
            union {                                                         \
                NK_CAT(f, BITS) f;                                          \
                NK_CAT(u, BITS) i;                                          \
            } pun = {.f = val};                                             \
            nk_printf(out, "0x%" NK_CAT(PRIX, BITS), pun.i);                \
        }                                                                   \
    } while (0)

void printFloat32Exact(NkStream out, f32 val) {
    PRINT_EXACT_FLOAT(32);
}

void printFloat64Exact(NkStream out, f64 val) {
    PRINT_EXACT_FLOAT(64);
}
