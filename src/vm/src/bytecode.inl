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

#ifndef CAST
#define CAST_X(TYPE, VALUE_TYPE, TO) XE(cast, TYPE##_to_##TO)
#define CAST(TO) NUMERIC_ITERATE(CAST_X, TO)
#endif

// TODO Figure out a way to compress CAST with NUMERIC_ITERATE

CAST(i8)
CAST(u8)
CAST(i16)
CAST(u16)
CAST(i32)
CAST(u32)
CAST(i64)
CAST(u64)
CAST(f32)
CAST(f64)

#undef CAST

X(call)
XE(call, jmp)

X(mov)
XE(mov, 8)
XE(mov, 16)
XE(mov, 32)
XE(mov, 64)

X(lea)

#ifndef NUM
#define NUM_IT(TYPE, VALUE_TYPE, NAME) XE(NAME, TYPE)
#define NUM(NAME) \
    X(NAME)       \
    NUMERIC_ITERATE(NUM_IT, NAME)
#endif

#ifndef INT
#define INT_IT(TYPE, VALUE_TYPE, NAME) XE(NAME, TYPE)
#define INT(NAME) \
    X(NAME)       \
    NUMERIC_ITERATE_INT(INT_IT, NAME)
#endif

NUM(neg)
INT(compl )
NUM(not )

NUM(add)
NUM(sub)
NUM(mul)
NUM(div)
INT(mod)

INT(bitand)
INT(bitor)
INT(xor)
INT(lsh)
INT(rsh)

NUM(and)
NUM(or)

NUM(ge)
NUM(gt)
NUM(le)
NUM(lt)

#undef NUM
#undef NUM_IT
#undef INT
#undef INT_IT

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
