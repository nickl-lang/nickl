#ifndef X
#define X(...)
#endif

#ifndef XE
#define XE(NAME, VAR) X(NAME##_##VAR)
#endif

X(nop)
X(ret)

X(jmp)
X(jmpz)
X(jmpnz)

X(cast)

X(call)
XE(call, jmp)

X(mov)
X(lea)
X(neg)
X(compl )
X(not )

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

NUM_X(eq)
NUM_X(ge)
NUM_X(gt)
NUM_X(le)
NUM_X(lt)
NUM_X(ne)

#undef X
#undef XE

#undef NUM_X
