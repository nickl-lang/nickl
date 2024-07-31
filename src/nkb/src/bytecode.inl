#ifndef OP
#define OP(NAME)
#endif

#ifndef OPX
#define OPX(NAME, EXT) OP(NAME##_##EXT)
#endif

#ifndef SIZ_OP
#define SIZ_OP(NAME) \
    OP(NAME)         \
    OPX(NAME, 8)     \
    OPX(NAME, 16)    \
    OPX(NAME, 32)    \
    OPX(NAME, 64)
#endif

#ifndef INT_OP
#define INT_OP(NAME) \
    OP(NAME)         \
    OPX(NAME, i8)    \
    OPX(NAME, u8)    \
    OPX(NAME, i16)   \
    OPX(NAME, u16)   \
    OPX(NAME, i32)   \
    OPX(NAME, u32)   \
    OPX(NAME, i64)   \
    OPX(NAME, u64)
#endif

#ifndef FP2I_OP
#define FP2I_OP INT_OP
#endif

#ifndef NUM_OP
#define NUM_OP(NAME) \
    INT_OP(NAME)     \
    OPX(NAME, f32)   \
    OPX(NAME, f64)
#endif

#ifndef BOOL_NUM_OP
#define BOOL_NUM_OP(NAME) NUM_OP(NAME)
#endif

OP(nop)

OP(ret)

OP(jmp)
SIZ_OP(jmpz)
SIZ_OP(jmpnz)

OP(call)
OPX(call, jmp)
OPX(call, ext)
OPX(call, extv)

OP(ext)

OP(sext)
OPX(sext, 8_16)
OPX(sext, 8_32)
OPX(sext, 8_64)
OPX(sext, 16_32)
OPX(sext, 16_64)
OPX(sext, 32_64)

OP(zext)
OPX(zext, 8_16)
OPX(zext, 8_32)
OPX(zext, 8_64)
OPX(zext, 16_32)
OPX(zext, 16_64)
OPX(zext, 32_64)

OP(fext)

OP(trunc)
OPX(trunc, 16_8)
OPX(trunc, 32_8)
OPX(trunc, 64_8)
OPX(trunc, 32_16)
OPX(trunc, 64_16)
OPX(trunc, 64_32)

OP(ftrunc)

OP(fp2i)

FP2I_OP(fp2i_32)
FP2I_OP(fp2i_64)

OP(i2fp)

FP2I_OP(i2fp_32)
FP2I_OP(i2fp_64)

SIZ_OP(mov)
OP(lea)

NUM_OP(add)
NUM_OP(sub)
NUM_OP(mul)
NUM_OP(div)
INT_OP(mod)

INT_OP(and)
INT_OP(or)
INT_OP(xor)
INT_OP(lsh)
INT_OP(rsh)

BOOL_NUM_OP(cmp_eq)
BOOL_NUM_OP(cmp_ne)
BOOL_NUM_OP(cmp_lt)
BOOL_NUM_OP(cmp_le)
BOOL_NUM_OP(cmp_gt)
BOOL_NUM_OP(cmp_ge)

OP(syscall)

OPX(syscall, 0)
OPX(syscall, 1)
OPX(syscall, 2)
OPX(syscall, 3)
OPX(syscall, 4)
OPX(syscall, 5)
OPX(syscall, 6)

OP(label)
OP(comment)
OP(line)

#undef BOOL_NUM_OP
#undef NUM_OP
#undef FP2I_OP
#undef INT_OP
#undef SIZ_OP

#undef OPX
#undef OP
