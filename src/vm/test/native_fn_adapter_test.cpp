#include "native_fn_adapter.h"

#include <cstdarg>
#include <cstring>

#include <gtest/gtest.h>

#include "nk/common/allocator.hpp"
#include "nk/common/logger.h"
#include "nk/common/utils.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"
#include "nk/vm/value.h"

namespace {

// clang-format off
using i8  = int8_t ;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float  ;
using f64 = double ;

struct ivec3 { i64 data[3]; };
struct dvec3 { f64 data[3]; };

static i8    s_i8_val   ;
static i16   s_i16_val  ;
static i32   s_i32_val  ;
static i64   s_i64_val  ;
static f32   s_f32_val  ;
static f64   s_f64_val  ;
static ivec3 s_ivec3_val;
static dvec3 s_dvec3_val;

extern "C" void set_i8   (i8    val) { s_i8_val    = val; }
extern "C" void set_i16  (i16   val) { s_i16_val   = val; }
extern "C" void set_i32  (i32   val) { s_i32_val   = val; }
extern "C" void set_i64  (i64   val) { s_i64_val   = val; }
extern "C" void set_f32  (f32   val) { s_f32_val   = val; }
extern "C" void set_f64  (f64   val) { s_f64_val   = val; }
extern "C" void set_ivec3(ivec3 val) { s_ivec3_val = val; }
extern "C" void set_dvec3(dvec3 val) { s_dvec3_val = val; }

extern "C" i8    get_i8   () { return s_i8_val   ; }
extern "C" i16   get_i16  () { return s_i16_val  ; }
extern "C" i32   get_i32  () { return s_i32_val  ; }
extern "C" i64   get_i64  () { return s_i64_val  ; }
extern "C" f32   get_f32  () { return s_f32_val  ; }
extern "C" f64   get_f64  () { return s_f64_val  ; }
extern "C" ivec3 get_ivec3() { return s_ivec3_val; }
extern "C" dvec3 get_dvec3() { return s_dvec3_val; }
// clang-format on

extern "C" void set_variadic(char const *name, ...) {
    va_list ap;
    va_start(ap, name);
    defer {
        va_end(ap);
    };

    std::string str{name};

    // clang-format off
    if (str == "i8"   ) { s_i8_val    = va_arg(ap, i32  ); return; }
    if (str == "i16"  ) { s_i16_val   = va_arg(ap, i32  ); return; }
    if (str == "i32"  ) { s_i32_val   = va_arg(ap, i32  ); return; }
    if (str == "i64"  ) { s_i64_val   = va_arg(ap, i64  ); return; }
    if (str == "f32"  ) { s_f32_val   = va_arg(ap, f64  ); return; }
    if (str == "f64"  ) { s_f64_val   = va_arg(ap, f64  ); return; }
    if (str == "ivec3") { s_ivec3_val = va_arg(ap, ivec3); return; }
    if (str == "dvec3") { s_dvec3_val = va_arg(ap, dvec3); return; }
    // clang-format on
}

bool operator==(ivec3 const &lhs, ivec3 const &rhs) {
    return 0 == std::memcmp(&lhs, &rhs, sizeof(lhs));
}

bool operator==(dvec3 const &lhs, dvec3 const &rhs) {
    return 0 == std::memcmp(&lhs, &rhs, sizeof(lhs));
}

class native_fn_adapter : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({});

        m_alloc = nk_arena_getAllocator(&m_arena);

        auto void_t = alloct(nkt_get_tuple(m_alloc, nullptr, 0, 1));

        i8_t = alloct(nkt_get_numeric(Int8));
        i16_t = alloct(nkt_get_numeric(Int16));
        i32_t = alloct(nkt_get_numeric(Int32));
        i64_t = alloct(nkt_get_numeric(Int64));
        f32_t = alloct(nkt_get_numeric(Float32));
        f64_t = alloct(nkt_get_numeric(Float64));

        nktype_t ivec3_types[] = {i64_t, i64_t, i64_t};
        ivec3_t = alloct(nkt_get_tuple(m_alloc, ivec3_types, AR_SIZE(ivec3_types), 1));

        nktype_t dvec3_types[] = {f64_t, f64_t, f64_t};
        dvec3_t = alloct(nkt_get_tuple(m_alloc, dvec3_types, AR_SIZE(dvec3_types), 1));

        set_i8_t = alloct(nkt_get_fn({void_t, i8_t, NkCallConv_Cdecl, false}));
        set_i16_t = alloct(nkt_get_fn({void_t, i16_t, NkCallConv_Cdecl, false}));
        set_i32_t = alloct(nkt_get_fn({void_t, i32_t, NkCallConv_Cdecl, false}));
        set_i64_t = alloct(nkt_get_fn({void_t, i64_t, NkCallConv_Cdecl, false}));
        set_f32_t = alloct(nkt_get_fn({void_t, f32_t, NkCallConv_Cdecl, false}));
        set_f64_t = alloct(nkt_get_fn({void_t, f64_t, NkCallConv_Cdecl, false}));
        set_ivec3_t = alloct(nkt_get_fn({void_t, ivec3_t, NkCallConv_Cdecl, false}));
        set_dvec3_t = alloct(nkt_get_fn({void_t, dvec3_t, NkCallConv_Cdecl, false}));

        get_i8_t = alloct(nkt_get_fn({i8_t, void_t, NkCallConv_Cdecl, false}));
        get_i16_t = alloct(nkt_get_fn({i16_t, void_t, NkCallConv_Cdecl, false}));
        get_i32_t = alloct(nkt_get_fn({i32_t, void_t, NkCallConv_Cdecl, false}));
        get_i64_t = alloct(nkt_get_fn({i64_t, void_t, NkCallConv_Cdecl, false}));
        get_f32_t = alloct(nkt_get_fn({f32_t, void_t, NkCallConv_Cdecl, false}));
        get_f64_t = alloct(nkt_get_fn({f64_t, void_t, NkCallConv_Cdecl, false}));
        get_ivec3_t = alloct(nkt_get_fn({ivec3_t, void_t, NkCallConv_Cdecl, false}));
        get_dvec3_t = alloct(nkt_get_fn({dvec3_t, void_t, NkCallConv_Cdecl, false}));

        str_t = alloct(nkt_get_ptr(i8_t));
        auto set_variadic_args_t = alloct(nkt_get_tuple(m_alloc, &str_t, 1, 1));

        set_variadic_t = alloct(nkt_get_fn({void_t, set_variadic_args_t, NkCallConv_Cdecl, true}));
    }

    void TearDown() override {
        nk_arena_free(&m_arena);
    }

protected:
    nktype_t alloct(NkType type) {
        return new (nk_arena_alloc_t<NkType>(&m_arena)) NkType{type};
    }

protected:
    NkArena m_arena{};
    NkAllocator m_alloc;

    nktype_t str_t;

    nktype_t i8_t;
    nktype_t i16_t;
    nktype_t i32_t;
    nktype_t i64_t;
    nktype_t f32_t;
    nktype_t f64_t;
    nktype_t ivec3_t;
    nktype_t dvec3_t;

    nktype_t set_i8_t;
    nktype_t set_i16_t;
    nktype_t set_i32_t;
    nktype_t set_i64_t;
    nktype_t set_f32_t;
    nktype_t set_f64_t;
    nktype_t set_ivec3_t;
    nktype_t set_dvec3_t;

    nktype_t get_i8_t;
    nktype_t get_i16_t;
    nktype_t get_i32_t;
    nktype_t get_i64_t;
    nktype_t get_f32_t;
    nktype_t get_f64_t;
    nktype_t get_ivec3_t;
    nktype_t get_dvec3_t;

    nktype_t set_variadic_t;

    void *set_i8_fn = (void *)set_i8;
    void *set_i16_fn = (void *)set_i16;
    void *set_i32_fn = (void *)set_i32;
    void *set_i64_fn = (void *)set_i64;
    void *set_f32_fn = (void *)set_f32;
    void *set_f64_fn = (void *)set_f64;
    void *set_ivec3_fn = (void *)set_ivec3;
    void *set_dvec3_fn = (void *)set_dvec3;

    void *get_i8_fn = (void *)get_i8;
    void *get_i16_fn = (void *)get_i16;
    void *get_i32_fn = (void *)get_i32;
    void *get_i64_fn = (void *)get_i64;
    void *get_f32_fn = (void *)get_f32;
    void *get_f64_fn = (void *)get_f64;
    void *get_ivec3_fn = (void *)get_ivec3;
    void *get_dvec3_fn = (void *)get_dvec3;

    void *set_variadic_fn = (void *)set_variadic;
};

} // namespace

#define SET_TEST(TYPE, VAL)                                                        \
    do {                                                                           \
        TYPE _val = VAL;                                                           \
        auto args_t = alloct(nkt_get_tuple(m_alloc, &TYPE##_t, 1, 1));             \
        nk_native_invoke({&set_##TYPE##_fn, set_##TYPE##_t}, {}, {&_val, args_t}); \
        EXPECT_EQ(s_##TYPE##_val, VAL);                                            \
    } while (0)

TEST_F(native_fn_adapter, set) {
    SET_TEST(i8, 12);
    SET_TEST(i16, 34);
    SET_TEST(i32, 56);
    SET_TEST(i64, 78);
    SET_TEST(f32, 3.14f);
    SET_TEST(f64, 3.14);
    SET_TEST(ivec3, (ivec3{1, 2, 3}));
    SET_TEST(dvec3, (dvec3{1.2, 3.4, 5.6}));
}

#define GET_TEST(TYPE, VAL)                                                          \
    do {                                                                             \
        s_##TYPE##_val = VAL;                                                        \
        TYPE _val{};                                                                 \
        nk_native_invoke({&get_##TYPE##_fn, get_##TYPE##_t}, {&_val, TYPE##_t}, {}); \
        EXPECT_EQ(_val, VAL);                                                        \
    } while (0)

TEST_F(native_fn_adapter, get) {
    GET_TEST(i8, 12);
    GET_TEST(i16, 34);
    GET_TEST(i32, 56);
    GET_TEST(i64, 78);
    GET_TEST(f32, 3.14f);
    GET_TEST(f64, 3.14);
    GET_TEST(ivec3, (ivec3{1, 2, 3}));
    GET_TEST(dvec3, (dvec3{1.2, 3.4, 5.6}));
}

template <class T>
struct VariadicArgs {
    char const *name;
    T val;
};

#define SET_VARIADIC_TEST(TYPE, VAL)                                                \
    do {                                                                            \
        nktype_t types[] = {str_t, TYPE##_t};                                       \
        auto args_t = alloct(nkt_get_tuple(m_alloc, types, AR_SIZE(types), 1));     \
        VariadicArgs<TYPE> _args{#TYPE, VAL};                                       \
        nk_native_invoke({&set_variadic_fn, set_variadic_t}, {}, {&_args, args_t}); \
        EXPECT_EQ(s_##TYPE##_val, VAL);                                             \
    } while (0)

TEST_F(native_fn_adapter, set_variadic) {
    SET_VARIADIC_TEST(i8, 12);
    SET_VARIADIC_TEST(i16, 34);
    SET_VARIADIC_TEST(i32, 56);
    SET_VARIADIC_TEST(i64, 78);
    // TODO(promotion) SET_VARIADIC_TEST(f32, 3.14f);
    SET_VARIADIC_TEST(f64, 3.14);
    SET_VARIADIC_TEST(ivec3, (ivec3{1, 2, 3}));
    SET_VARIADIC_TEST(dvec3, (dvec3{1.2, 3.4, 5.6}));
}
