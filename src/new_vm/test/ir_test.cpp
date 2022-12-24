#include "nk/vm/ir.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>

#include <gtest/gtest.h>

#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/string.hpp"
#include "nk/common/string_builder.h"
#include "nk/common/utils.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"
#include "nk/vm/value.h"

namespace {

NK_LOG_USE_SCOPE(test);

class ir : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({});

        m_arena = nk_create_arena();
    }

    void TearDown() override {
        nk_free_arena(m_arena);
    }

protected:
    void inspect(NkIrProg p) {
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };

        nkir_inspect(p, sb);
        auto str = nksb_concat(sb);

        NK_LOG_INF("ir:\n%.*s", str.size, str.data);
    }

protected:
    NkAllocator *m_arena;
};

} // namespace

TEST_F(ir, add) {
    auto p = nkir_createProgram();
    defer {
        nkir_deinitProgram(p);
    };

    auto i32_t = nkt_get_numeric(m_arena, Int32);

    nktype_t args_types[] = {i32_t, i32_t};
    auto args_t = nkt_get_tuple(m_arena, args_types, AR_SIZE(args_types), 1);

    auto add = nkir_makeFunct(p);
    auto add_fn_t = nkt_get_fn(m_arena, i32_t, args_t, NkCallConv_Nk, false);
    nkir_startFunct(p, add, cs2s("add"), add_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), cs2s("start"));

    nkir_gen(p, nkir_make_add(nkir_makeRetRef(p), nkir_makeArgRef(p, 0), nkir_makeArgRef(p, 1)));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    int32_t args[] = {4, 5};
    int32_t res = 0;

    nkir_invoke({&add, add_fn_t}, {&res, i32_t}, {args, args_t});

    EXPECT_EQ(res, 9);
}

TEST_F(ir, nested_functions) {
    auto p = nkir_createProgram();
    defer {
        nkir_deinitProgram(p);
    };

    auto i32_t = nkt_get_numeric(m_arena, Int32);

    int32_t const_2 = 2;
    int32_t const_4 = 4;

    auto getEight = nkir_makeFunct(p);
    auto getEight_fn_t =
        nkt_get_fn(m_arena, i32_t, nkt_get_tuple(m_arena, nullptr, 0, 1), NkCallConv_Nk, false);
    nkir_startFunct(p, getEight, cs2s("getEight"), getEight_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), cs2s("start"));

    auto getFour = nkir_makeFunct(p);
    auto getFour_fn_t =
        nkt_get_fn(m_arena, i32_t, nkt_get_tuple(m_arena, nullptr, 0, 1), NkCallConv_Nk, false);
    nkir_startFunct(p, getFour, cs2s("getFour"), getFour_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), cs2s("start"));

    nkir_gen(p, nkir_make_mov(nkir_makeRetRef(p), nkir_makeConstRef(p, {&const_4, i32_t})));
    nkir_gen(p, nkir_make_ret());

    nkir_activateFunct(p, getEight);

    auto var = nkir_makeFrameRef(p, nkir_makeLocalVar(p, i32_t));

    nkir_gen(p, nkir_make_call(var, nkir_makeFunctRef(p, getFour), {}));
    nkir_gen(p, nkir_make_mul(nkir_makeRetRef(p), var, nkir_makeConstRef(p, {&const_2, i32_t})));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    int32_t res = 0;
    nkir_invoke({&getEight, getEight_fn_t}, {&res, i32_t}, {});
    EXPECT_EQ(res, 8);
}

TEST_F(ir, isEven) {
    auto p = nkir_createProgram();
    defer {
        nkir_deinitProgram(p);
    };

    auto i32_t = nkt_get_numeric(m_arena, Int32);

    auto args_t = nkt_get_tuple(m_arena, &i32_t, 1, 1);

    auto isEven = nkir_makeFunct(p);
    auto isEven_fn_t = nkt_get_fn(m_arena, i32_t, args_t, NkCallConv_Nk, false);
    nkir_startFunct(p, isEven, cs2s("isEven"), isEven_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), cs2s("start"));

    int32_t const_0 = 0;
    int32_t const_1 = 1;
    int32_t const_2 = 2;

    auto l_else = nkir_makeBlock(p);
    auto l_end = nkir_makeBlock(p);

    auto var = nkir_makeFrameRef(p, nkir_makeLocalVar(p, i32_t));

    nkir_gen(p, nkir_make_mod(var, nkir_makeArgRef(p, 0), nkir_makeConstRef(p, {&const_2, i32_t})));
    nkir_gen(p, nkir_make_jmpnz(var, l_else));
    nkir_gen(p, nkir_make_mov(nkir_makeRetRef(p), nkir_makeConstRef(p, {&const_1, i32_t})));
    nkir_gen(p, nkir_make_jmp(l_end));
    nkir_startBlock(p, l_else, cs2s("else"));
    nkir_gen(p, nkir_make_mov(nkir_makeRetRef(p), nkir_makeConstRef(p, {&const_0, i32_t})));
    nkir_startBlock(p, l_end, cs2s("end"));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    int32_t res = 42;
    int32_t args[] = {0};

    args[0] = 1;
    nkir_invoke({&isEven, isEven_fn_t}, {&res, i32_t}, {&args, args_t});
    EXPECT_EQ(res, 0);

    args[0] = 2;
    nkir_invoke({&isEven, isEven_fn_t}, {&res, i32_t}, {&args, args_t});
    EXPECT_EQ(res, 1);
}

static char const *s_test_print_str;
extern "C" NK_EXPORT void _test_print(char const *str) {
    s_test_print_str = str;
    puts(str);
}

TEST_F(ir, native_call) {
    auto p = nkir_createProgram();
    defer {
        nkir_deinitProgram(p);
    };

    auto void_t = nkt_get_void(m_arena);
    auto i8_t = nkt_get_numeric(m_arena, Int8);
    auto i8_ptr_t = nkt_get_ptr(m_arena, i8_t);

    auto sayHello = nkir_makeFunct(p);
    auto sayHello_fn_t =
        nkt_get_fn(m_arena, void_t, nkt_get_tuple(m_arena, nullptr, 0, 1), NkCallConv_Nk, false);
    nkir_startFunct(p, sayHello, cs2s("sayHello"), sayHello_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), cs2s("start"));

    char const *const_str = "Hello, World!";

    auto test_print_args_t = nkt_get_tuple(m_arena, &i8_ptr_t, 1, 1);

    auto ar_t = nkt_get_array(m_arena, i8_t, std::strlen(const_str) + 1);
    auto str_t = nkt_get_ptr(m_arena, ar_t);
    auto actual_args_t = nkt_get_tuple(m_arena, &str_t, 1, 1);

    auto so = nkir_makeShObj(p, cs2s(""));
    auto print_fn = nkir_makeExtSym(
        p,
        so,
        cs2s("_test_print"),
        nkt_get_fn(m_arena, void_t, test_print_args_t, NkCallConv_Cdecl, false));

    nkir_gen(
        p,
        nkir_make_call(
            {},
            nkir_makeExtSymRef(p, print_fn),
            nkir_makeConstRef(p, {&const_str, actual_args_t})));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    nkir_invoke({&sayHello, sayHello_fn_t}, {}, {});

    EXPECT_STREQ(s_test_print_str, const_str);
}

extern "C" NK_EXPORT uint32_t _test_log2(uint32_t x) {
    return log2u(x);
}

extern "C" NK_EXPORT void _test_fillAr(uint8_t *buf, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        buf[i] = i;
    }
}

static uint32_t s_test_ar_sum;
extern "C" NK_EXPORT void _test_printAr(uint8_t *buf, uint32_t size) {
    s_test_ar_sum = 0;
    for (uint32_t i = 0; i < size; i++) {
        if (i > 0) {
            printf(", ");
        }
        printf("%i", (int)buf[i]);
        s_test_ar_sum += buf[i];
    }
    puts("");
}

TEST_F(ir, nested_functions_call_while_compiling) {
    auto p = nkir_createProgram();
    defer {
        nkir_deinitProgram(p);
    };

    auto u8_t = nkt_get_numeric(m_arena, Uint8);
    auto u8_ptr_t = nkt_get_ptr(m_arena, u8_t);
    auto u32_t = nkt_get_numeric(m_arena, Uint32);
    auto void_t = nkt_get_void(m_arena);

    auto so = nkir_makeShObj(p, cs2s(""));

    auto log2_args_t = nkt_get_tuple(m_arena, &u32_t, 1, 1);
    auto log2_fn = nkir_makeExtSym(
        p,
        so,
        cs2s("_test_log2"),
        nkt_get_fn(m_arena, u32_t, log2_args_t, NkCallConv_Cdecl, false));

    nktype_t args_types[] = {u8_ptr_t, u32_t};
    auto args_t = nkt_get_tuple(m_arena, args_types, AR_SIZE(args_types), 1);

    auto fillAr_fn = nkir_makeExtSym(
        p, so, cs2s("_test_fillAr"), nkt_get_fn(m_arena, void_t, args_t, NkCallConv_Cdecl, false));

    auto printAr_fn = nkir_makeExtSym(
        p, so, cs2s("_test_printAr"), nkt_get_fn(m_arena, void_t, args_t, NkCallConv_Cdecl, false));

    auto test = nkir_makeFunct(p);
    auto test_fn_t =
        nkt_get_fn(m_arena, u32_t, nkt_get_tuple(m_arena, nullptr, 0, 1), NkCallConv_Nk, false);
    nkir_startFunct(p, test, cs2s("test"), test_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), cs2s("start"));

    auto getArrSize = nkir_makeFunct(p);
    auto getArrSize_fn_t =
        nkt_get_fn(m_arena, u32_t, nkt_get_tuple(m_arena, nullptr, 0, 1), NkCallConv_Nk, false);
    nkir_startFunct(p, getArrSize, cs2s("getArrSize"), getArrSize_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), cs2s("start"));

    uint32_t const_64 = 64;

    nkir_gen(
        p,
        nkir_make_call(
            nkir_makeRetRef(p),
            nkir_makeExtSymRef(p, log2_fn),
            nkir_makeConstRef(p, {&const_64, log2_args_t})));
    nkir_gen(p, nkir_make_ret());

    uint32_t ar_size = 0;
    nkir_invoke({&getArrSize, getArrSize_fn_t}, {&ar_size, u32_t}, {});
    EXPECT_EQ(ar_size, 6);

    nkir_activateFunct(p, test);

    auto ar_t = nkt_get_array(m_arena, u8_t, ar_size);

    auto ar = nkir_makeFrameRef(p, nkir_makeLocalVar(p, ar_t));
    auto args = nkir_makeFrameRef(p, nkir_makeLocalVar(p, args_t));

    auto buf_arg = args;
    buf_arg.type = args_types[0];
    buf_arg.offset = args_t->as.tuple.elems.data[0].offset;
    auto size_arg = args;
    size_arg.type = args_types[1];
    size_arg.offset = args_t->as.tuple.elems.data[1].offset;

    nkir_gen(p, nkir_make_lea(buf_arg, ar));
    nkir_gen(p, nkir_make_mov(size_arg, nkir_makeConstRef(p, {(void *)&ar_t->size, u32_t})));
    nkir_gen(p, nkir_make_call({}, nkir_makeExtSymRef(p, fillAr_fn), args));
    nkir_gen(p, nkir_make_call({}, nkir_makeExtSymRef(p, printAr_fn), args));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    nkir_invoke({&test, test_fn_t}, {}, {});

    EXPECT_EQ(s_test_ar_sum, 15);
}
