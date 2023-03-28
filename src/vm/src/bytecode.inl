#ifndef X
#define X(...)
#endif

#ifndef XE
#define XE(NAME, VAR) X(NAME##_##VAR)
#endif

X(nop)
X(ret)
X(enter)
X(leave)

X(jmp)

X(jmpz)
XE(jmpz, 8)
XE(jmpz, 16)
XE(jmpz, 32)
XE(jmpz, 64)

X(jmpnz)
XE(jmpnz, 8)
XE(jmpnz, 16)
XE(jmpnz, 32)
XE(jmpnz, 64)

X(cast)

#ifndef CAST_X
#define CAST_X(TO)        \
    XE(cast, i8_to_##TO)  \
    XE(cast, u8_to_##TO)  \
    XE(cast, i16_to_##TO) \
    XE(cast, u16_to_##TO) \
    XE(cast, i32_to_##TO) \
    XE(cast, u32_to_##TO) \
    XE(cast, i64_to_##TO) \
    XE(cast, u64_to_##TO) \
    XE(cast, f32_to_##TO) \
    XE(cast, f64_to_##TO)
#endif

CAST_X(i8)
CAST_X(u8)
CAST_X(i16)
CAST_X(u16)
CAST_X(i32)
CAST_X(u32)
CAST_X(i64)
CAST_X(u64)
CAST_X(f32)
CAST_X(f64)

#undef CAST_X

X(call)
XE(call, jmp)

X(mov)
XE(mov, 8)
XE(mov, 16)
XE(mov, 32)
XE(mov, 64)

X(lea)

#ifndef NUM_X
#define NUM_X(NAME) \
    X(NAME)         \
    XE(NAME, i8)    \
    XE(NAME, u8)    \
    XE(NAME, i16)   \
    XE(NAME, u16)   \
    XE(NAME, i32)   \
    XE(NAME, u32)   \
    XE(NAME, i64)   \
    XE(NAME, u64)   \
    XE(NAME, f32)   \
    XE(NAME, f64)
#endif

#ifndef NUM_INT_X
#define NUM_INT_X(NAME) \
    X(NAME)             \
    XE(NAME, i8)        \
    XE(NAME, u8)        \
    XE(NAME, i16)       \
    XE(NAME, u16)       \
    XE(NAME, i32)       \
    XE(NAME, u32)       \
    XE(NAME, i64)       \
    XE(NAME, u64)
#endif

NUM_X(neg)
NUM_INT_X(compl )
NUM_X(not )

NUM_X(add)
NUM_X(sub)
NUM_X(mul)
NUM_X(div)
NUM_INT_X(mod)

NUM_INT_X(bitand)
NUM_INT_X(bitor)
NUM_INT_X(xor)
NUM_INT_X(lsh)
NUM_INT_X(rsh)

NUM_X(and)
NUM_X(or)

NUM_X(ge)
NUM_X(gt)
NUM_X(le)
NUM_X(lt)

X(eq)
XE(eq, 8)
XE(eq, 16)
XE(eq, 32)
XE(eq, 64)

X(ne)
XE(ne, 8)
XE(ne, 16)
XE(ne, 32)
XE(ne, 64)

#undef X
#undef XE

#undef NUM_X
#undef NUM_INT_X
