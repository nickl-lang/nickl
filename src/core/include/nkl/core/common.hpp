#ifndef HEADER_GUARD_NKL_CORE_COMMON
#define HEADER_GUARD_NKL_CORE_COMMON

#include <cstddef>
#include <cstring>
#include <string_view>

#include "nk/common/common.hpp"

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

#endif // HEADER_GUARD_NKL_CORE_COMMON
