#include "nk/vm/ir.h"

#include <cstdint>
#include <cstdio>
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

    auto var = nkir_makeLocalVar(p, i32_t);

    nkir_gen(p, nkir_make_call(nkir_makeFrameRef(p, var), nkir_makeFunctRef(p, getFour), {}));
    nkir_gen(
        p,
        nkir_make_mul(
            nkir_makeRetRef(p),
            nkir_makeFrameRef(p, var),
            nkir_makeConstRef(p, {&const_2, i32_t})));
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

    nktype_t args_types[] = {i32_t};
    auto args_t = nkt_get_tuple(m_arena, args_types, AR_SIZE(args_types), 1);

    auto isEven = nkir_makeFunct(p);
    auto isEven_fn_t = nkt_get_fn(m_arena, i32_t, args_t, NkCallConv_Nk, false);
    nkir_startFunct(p, isEven, cs2s("isEven"), isEven_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), cs2s("start"));

    int32_t const_0 = 0;
    int32_t const_1 = 1;
    int32_t const_2 = 2;

    auto l_else = nkir_makeBlock(p);
    auto l_end = nkir_makeBlock(p);

    auto var = nkir_makeLocalVar(p, i32_t);

    nkir_gen(
        p,
        nkir_make_mod(
            nkir_makeFrameRef(p, var),
            nkir_makeArgRef(p, 0),
            nkir_makeConstRef(p, {&const_2, i32_t})));
    nkir_gen(p, nkir_make_jmpnz(nkir_makeFrameRef(p, var), l_else));
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

    nktype_t args_types[] = {i8_ptr_t};
    auto test_print_args_t = nkt_get_tuple(m_arena, args_types, AR_SIZE(args_types), 1);

    auto so = nkir_makeShObj(p, cs2s(""));
    auto _test_print_fn = nkir_makeExtSym(
        p,
        so,
        cs2s("_test_print"),
        nkt_get_fn(m_arena, void_t, test_print_args_t, NkCallConv_Cdecl, false));

    nkir_gen(
        p,
        nkir_make_call(
            {},
            nkir_makeExtSymRef(p, _test_print_fn),
            nkir_makeConstRef(p, {&const_str, test_print_args_t})));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    nkir_invoke({&sayHello, sayHello_fn_t}, {}, {});

    EXPECT_STREQ(s_test_print_str, const_str);
}
