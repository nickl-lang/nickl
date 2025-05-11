#ifndef NKB_TYPES_H_
#define NKB_TYPES_H_

#include "ntk/common.h"
#include "ntk/slice.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkIrType_T const *NkIrType;

typedef enum {
    NkIrType_Aggregate,
    NkIrType_Numeric,
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
} NkIrNumericValueType;

#define NKIR_NUMERIC_TYPE_SIZE(VALUE_TYPE) (0xf & (VALUE_TYPE))
#define NKIR_NUMERIC_TYPE_INDEX(VALUE_TYPE) ((0xf0 & (VALUE_TYPE)) >> 4)
#define NKIR_NUMERIC_TYPE_COUNT 10

#define NKIR_NUMERIC_IS_INT(VALUE_TYPE) ((VALUE_TYPE) >= Int8 && (VALUE_TYPE) <= Uint64)
#define NKIR_NUMERIC_IS_FLT(VALUE_TYPE) ((VALUE_TYPE) >= Float32 && (VALUE_TYPE) <= Float64)

#define NKIR_NUMERIC_IS_SIGNED(VALUE_TYPE) ((NKIR_NUMERIC_TYPE_INDEX(VALUE_TYPE) & 1) == 0)

#define NKIR_NUMERIC_ITERATE_INT(X, ...)      \
    X(i8, Int8 __VA_OPT__(, ) __VA_ARGS__)    \
    X(u8, Uint8 __VA_OPT__(, ) __VA_ARGS__)   \
    X(i16, Int16 __VA_OPT__(, ) __VA_ARGS__)  \
    X(u16, Uint16 __VA_OPT__(, ) __VA_ARGS__) \
    X(i32, Int32 __VA_OPT__(, ) __VA_ARGS__)  \
    X(u32, Uint32 __VA_OPT__(, ) __VA_ARGS__) \
    X(i64, Int64 __VA_OPT__(, ) __VA_ARGS__)  \
    X(u64, Uint64 __VA_OPT__(, ) __VA_ARGS__)

#define NKIR_NUMERIC_ITERATE_FLT(X, ...)       \
    X(f32, Float32 __VA_OPT__(, ) __VA_ARGS__) \
    X(f64, Float64 __VA_OPT__(, ) __VA_ARGS__)

#define NKIR_NUMERIC_ITERATE(X, ...)                       \
    NKIR_NUMERIC_ITERATE_INT(X __VA_OPT__(, ) __VA_ARGS__) \
    NKIR_NUMERIC_ITERATE_FLT(X __VA_OPT__(, ) __VA_ARGS__)

typedef struct {
    NkIrType type;
    u32 count;
    u32 offset;
} NkIrAggregateElemInfo;

typedef NkSlice(NkIrAggregateElemInfo const) NkIrAggregateElemInfoArray;

typedef struct NkIrType_T {
    union {
        NkIrNumericValueType num;
        NkIrAggregateElemInfoArray aggr;
    };
    u64 size;
    u32 align;
    u32 id;
    NkIrTypeKind kind;
} NkIrType_T;

void nkir_inspectType(NkIrType type, NkStream out);
void nkir_inspectVal(void *data, NkIrType type, NkStream out);

#ifdef __cplusplus
}
#endif

#endif // NKB_TYPES_H_
