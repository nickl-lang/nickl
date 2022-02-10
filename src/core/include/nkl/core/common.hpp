#ifndef HEADER_GUARD_NKL_CORE_COMMON
#define HEADER_GUARD_NKL_CORE_COMMON

namespace nkl {

#include <cstdint>

union NumericVariant {
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    float f32;
    double f64;
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_COMMON
