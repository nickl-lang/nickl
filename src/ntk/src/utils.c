#include "ntk/utils.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>

#include "ntk/string_builder.h"

#define HASH_ARRAY_STEP sizeof(usize)

u64 nk_hashArray(u8 const *begin, u8 const *end) {
    u64 hash = 0;
    usize i = 0;
    while (begin + (i + 1) * HASH_ARRAY_STEP <= end) {
        nk_hashCombine(&hash, *(usize const *)(begin + i++ * HASH_ARRAY_STEP));
    }
    i *= HASH_ARRAY_STEP;
    while (begin + i < end) {
        nk_hashCombine(&hash, begin[i++]);
    }
    return hash;
}

// TODO: Exclude trailing zeroes where possible

#define F32_DECIMAL_DIG FLT_DECIMAL_DIG
#define F64_DECIMAL_DIG DBL_DECIMAL_DIG

#define strtof32 strtof
#define strtof64 strtod

#define PRINT_FLOAT_EXACT(OUT, VAL, BITS)                                         \
    do {                                                                          \
        NKSB_FIXED_BUFFER(buf, 50);                                               \
        NkStream const out_buf = nksb_getStream(&buf);                            \
                                                                                  \
        nk_printf(out_buf, "%.*e", NK_CAT(NK_CAT(F, BITS), _DECIMAL_DIG), (VAL)); \
        nksb_appendNull(&buf);                                                    \
        char const *cstr = buf.data;                                              \
                                                                                  \
        NK_CAT(f, BITS) const expected = (VAL);                                   \
        char *endptr = NULL;                                                      \
        NK_CAT(f, BITS) const actual = NK_CAT(strtof, BITS)(cstr, &endptr);       \
                                                                                  \
        nk_assert(endptr == cstr + buf.size - 1);                                 \
                                                                                  \
        if (expected == actual) {                                                 \
            nk_printf((OUT), "%s", cstr);                                         \
                                                                                  \
            char const *p = cstr;                                                 \
            for (; *p; p++) {                                                     \
                if (*p == '.') {                                                  \
                    break;                                                        \
                }                                                                 \
            }                                                                     \
            if (!*p) {                                                            \
                nk_printf((OUT), ".0");                                           \
            }                                                                     \
        } else {                                                                  \
            union {                                                               \
                NK_CAT(f, BITS) f;                                                \
                NK_CAT(u, BITS) i;                                                \
            } pun = {.f = (VAL)};                                                 \
            nk_printf((OUT), "0x%" NK_CAT(PRIX, BITS), pun.i);                    \
        }                                                                         \
    } while (0)

void printFloat32Exact(NkStream out, f32 val) {
    PRINT_FLOAT_EXACT(out, val, 32);
}

void printFloat64Exact(NkStream out, f64 val) {
    PRINT_FLOAT_EXACT(out, val, 64);
}
