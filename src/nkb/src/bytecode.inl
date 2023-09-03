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

#ifndef NUM_OP
#define NUM_OP(NAME) \
    INT_OP(NAME)     \
    OPX(NAME, f32)   \
    OPX(NAME, f64)
#endif

OP(nop)

OP(ret)

OP(jmp)
SIZ_OP(jmpz)
SIZ_OP(jmpnz)

OP(call)
OPX(call, jmp)
OPX(call, ext)

OP(ext)
OPX(ext, i8_16)
OPX(ext, i8_32)
OPX(ext, i8_64)
OPX(ext, i16_32)
OPX(ext, i16_64)
OPX(ext, i32_64)
OPX(ext, u8_16)
OPX(ext, u8_32)
OPX(ext, u8_64)
OPX(ext, u16_32)
OPX(ext, u16_64)
OPX(ext, u32_64)
OPX(ext, f32_f64)

OP(trunc)
OPX(trunc, u16_8)
OPX(trunc, u32_8)
OPX(trunc, u32_16)
OPX(trunc, u64_8)
OPX(trunc, u64_16)
OPX(trunc, u64_32)
OPX(trunc, f64_32)

OP(fp2i)

OPX(fp2i_f32, i8)
OPX(fp2i_f32, u8)
OPX(fp2i_f32, i16)
OPX(fp2i_f32, u16)
OPX(fp2i_f32, i32)
OPX(fp2i_f32, u32)
OPX(fp2i_f32, i64)
OPX(fp2i_f32, u64)

OPX(fp2i_f64, i8)
OPX(fp2i_f64, u8)
OPX(fp2i_f64, i16)
OPX(fp2i_f64, u16)
OPX(fp2i_f64, i32)
OPX(fp2i_f64, u32)
OPX(fp2i_f64, i64)
OPX(fp2i_f64, u64)

OP(i2fp)

OPX(i2fp_f32, i8)
OPX(i2fp_f32, u8)
OPX(i2fp_f32, i16)
OPX(i2fp_f32, u16)
OPX(i2fp_f32, i32)
OPX(i2fp_f32, u32)
OPX(i2fp_f32, i64)
OPX(i2fp_f32, u64)

OPX(i2fp_f64, i8)
OPX(i2fp_f64, u8)
OPX(i2fp_f64, i16)
OPX(i2fp_f64, u16)
OPX(i2fp_f64, i32)
OPX(i2fp_f64, u32)
OPX(i2fp_f64, i64)
OPX(i2fp_f64, u64)

NUM_OP(neg)

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

NUM_OP(cmp_eq)
NUM_OP(cmp_ne)
NUM_OP(cmp_lt)
NUM_OP(cmp_le)
NUM_OP(cmp_gt)
NUM_OP(cmp_ge)

OP(label)

#undef NUM_OP
#undef INT_OP
#undef SIZ_OP

#undef OPX
#undef OP
