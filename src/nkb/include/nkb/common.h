#ifndef HEADER_GUARD_NKB_COMMON
#define HEADER_GUARD_NKB_COMMON

#include <stddef.h>
#include <stdint.h>

#include "nk/common/string_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NkbOutput_None,

    NkbOutput_Object,
    NkbOutput_Static,
    NkbOutput_Shared,
    NkbOutput_Executable,
} NkbOutputKind;

typedef struct NkIrType const *nktype_t;

typedef enum {
    NkType_Basic,
    NkType_Aggregate,

    NkTypeKind_Count,
} NkIrTypeKind;

// 0x<index><size>
// Ordered in coercion order, max(lhs, rhs) is a common type
typedef enum {
    Int8 = 0x01,
    Uint8 = 0x11,
    Int16 = 0x22,
    Uint16 = 0x32,
    Int32 = 0x44,
    Uint32 = 0x54,
    Int64 = 0x68,
    Uint64 = 0x78,
    Float32 = 0x84,
    Float64 = 0x98,
} NkIrBasicValueType;

#define NKIR_BASIC_TYPE_SIZE(VALUE_TYPE) (0xf & (VALUE_TYPE))
#define NKIR_BASIC_TYPE_INDEX(VALUE_TYPE) ((0xf0 & (VALUE_TYPE)) >> 4)
#define NKIR_BASIC_TYPE_COUNT 10

#define NKIR_NUMERIC_ITERATE_INT(MACRO, ...)                \
    MACRO(i8, Int8, int8_t __VA_OPT__(, ) __VA_ARGS__)      \
    MACRO(u8, Uint8, uint8_t __VA_OPT__(, ) __VA_ARGS__)    \
    MACRO(i16, Int16, int16_t __VA_OPT__(, ) __VA_ARGS__)   \
    MACRO(u16, Uint16, uint16_t __VA_OPT__(, ) __VA_ARGS__) \
    MACRO(i32, Int32, int32_t __VA_OPT__(, ) __VA_ARGS__)   \
    MACRO(u32, Uint32, uint32_t __VA_OPT__(, ) __VA_ARGS__) \
    MACRO(i64, Int64, int64_t __VA_OPT__(, ) __VA_ARGS__)   \
    MACRO(u64, Uint64, uint64_t __VA_OPT__(, ) __VA_ARGS__)

#define NKIR_NUMERIC_ITERATE(MACRO, ...)                       \
    NKIR_NUMERIC_ITERATE_INT(MACRO __VA_OPT__(, ) __VA_ARGS__) \
    MACRO(f32, Float32, float __VA_OPT__(, ) __VA_ARGS__)      \
    MACRO(f64, Float64, double __VA_OPT__(, ) __VA_ARGS__)

typedef struct {
    nktype_t type;
    size_t count;
    size_t offset;
} NkIrAggregateElemInfo;

typedef struct {
    NkIrAggregateElemInfo const *data;
    size_t size;
} NkIrAggregateElemInfoArray;

typedef struct {
    NkIrBasicValueType value_type;
} NkIrBasicTypeInfo;

typedef struct {
    NkIrAggregateElemInfoArray elems;
} NkIrAggregateTypeInfo;

typedef struct NkIrType {
    union {
        NkIrBasicTypeInfo basic;
        NkIrAggregateTypeInfo aggregate;
    } as;
    uint64_t size;
    uint8_t align;
    NkIrTypeKind kind;
} NkIrType;

void nkirt_inspect(nktype_t type, NkStringBuilder sb);
void nkirv_inspect(void *data, nktype_t type, NkStringBuilder sb);

// TODO Move somewhere
#define DEFINE_ID_TYPE(NAME) \
    typedef struct {         \
        size_t id;           \
    } NAME

#define INVALID_ID \
    size_t {       \
        -1ul       \
    }

typedef struct {
    NkIrAggregateElemInfoArray info_ar;
    size_t size;
    size_t align;
} NkIrAggregateLayout;

NkIrAggregateLayout nkir_calcAggregateLayout(
    NkAllocator alloc,
    nktype_t const *elem_types,
    size_t const *elem_counts,
    size_t n,
    size_t stride);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_COMMON