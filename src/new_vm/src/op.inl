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
X(jmpnz)

//@Todo Implement extensions for cast
X(cast)

X(call)
XE(call, jmp)

X(mov)
XE(mov, 8)
XE(mov, 16)
XE(mov, 32)
XE(mov, 64)

X(lea)
X(neg)
X(compl )
X(not )

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
