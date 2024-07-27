#include "nk/vm/ir.h"

#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

#include "native_fn_adapter.h"
#include "nk/vm/common.h"
#include "nk/vm/value.h"
#include "ntk/allocator.h"
#include "ntk/common.h"
#include "ntk/log.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(test);

nktype_t alloct(NkArena *arena, NkType type) {
    return new (nk_arena_allocAligned(arena, sizeof(NkType), alignof(NkType))) NkType{type};
}

class ir : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});

        m_alloc = nk_arena_getAllocator(&m_arena);
    }

    void TearDown() override {
        nk_arena_free(&m_arena);
        nk_native_adapterDeinit();
    }

protected:
    nktype_t alloct(NkType type) {
        return ::alloct(&m_arena, type);
    }

    void inspect(NkIrProg p) {
        (void)p;
#ifdef ENABLE_LOGGING
        NkStringBuilder sb{};
        defer {
            nksb_free(&sb);
        };
        nkir_inspect(p, &sb);
        NK_LOG_INF("ir:\n" NKS_FMT, NKS_ARG(sb));
#endif // ENABLE_LOGGING
    }

protected:
    NkArena m_arena{};
    NkAllocator m_alloc{};
};

} // namespace

TEST_F(ir, add) {
    auto p = nkir_createProgram();
    defer {
        nkir_deinitProgram(p);
    };

    auto i32_t = alloct(nkt_get_numeric(Int32));

    nktype_t args_types[] = {i32_t, i32_t};
    auto args_t = alloct(nkt_get_tuple(m_alloc, args_types, NK_ARRAY_COUNT(args_types), sizeof(nktype_t)));

    auto add = nkir_makeFunct(p);
    auto add_fn_t = alloct(nkt_get_fn({i32_t, args_t, NkCallConv_Nk, false}));
    nkir_startFunct(add, nk_cs2s("add"), add_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), nk_cs2s("start"));

    nkir_gen(p, nkir_make_add(nkir_makeRetRef(p), nkir_makeArgRef(p, 0), nkir_makeArgRef(p, 1)));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    i32 args[] = {4, 5};
    i32 res = 0;

    nkir_invoke({&add, add_fn_t}, {&res, i32_t}, {args, args_t});

    EXPECT_EQ(res, 9);
}

TEST_F(ir, nested_functions) {
    auto p = nkir_createProgram();
    defer {
        nkir_deinitProgram(p);
    };

    auto i32_t = alloct(nkt_get_numeric(Int32));

    i32 const_2 = 2;
    i32 const_4 = 4;

    auto getEight = nkir_makeFunct(p);
    auto getEight_fn_t =
        alloct(nkt_get_fn({i32_t, alloct(nkt_get_tuple(m_alloc, nullptr, 0, 0)), NkCallConv_Nk, false}));
    nkir_startFunct(getEight, nk_cs2s("getEight"), getEight_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), nk_cs2s("start"));

    auto getFour = nkir_makeFunct(p);
    auto getFour_fn_t =
        alloct(nkt_get_fn({i32_t, alloct(nkt_get_tuple(m_alloc, nullptr, 0, 0)), NkCallConv_Nk, false}));
    nkir_startFunct(getFour, nk_cs2s("getFour"), getFour_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), nk_cs2s("start"));

    nkir_gen(p, nkir_make_mov(nkir_makeRetRef(p), nkir_makeConstRef(p, nkir_makeConst(p, {&const_4, i32_t}))));
    nkir_gen(p, nkir_make_ret());

    nkir_activateFunct(p, getEight);

    auto var = nkir_makeFrameRef(p, nkir_makeLocalVar(p, i32_t));

    nkir_gen(p, nkir_make_call(var, nkir_makeConstRef(p, nkir_makeConst(p, {&getFour, getFour_fn_t})), {}));
    nkir_gen(p, nkir_make_mul(nkir_makeRetRef(p), var, nkir_makeConstRef(p, nkir_makeConst(p, {&const_2, i32_t}))));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    i32 res = 0;
    nkir_invoke({&getEight, getEight_fn_t}, {&res, i32_t}, {});
    EXPECT_EQ(res, 8);
}

TEST_F(ir, isEven) {
    auto p = nkir_createProgram();
    defer {
        nkir_deinitProgram(p);
    };

    auto i32_t = alloct(nkt_get_numeric(Int32));

    auto args_t = alloct(nkt_get_tuple(m_alloc, &i32_t, 1, 0));

    auto isEven = nkir_makeFunct(p);
    auto isEven_fn_t = alloct(nkt_get_fn({i32_t, args_t, NkCallConv_Nk, false}));
    nkir_startFunct(isEven, nk_cs2s("isEven"), isEven_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), nk_cs2s("start"));

    i32 const_0 = 0;
    i32 const_1 = 1;
    i32 const_2 = 2;

    auto l_else = nkir_makeBlock(p);
    auto l_end = nkir_makeBlock(p);

    auto var = nkir_makeFrameRef(p, nkir_makeLocalVar(p, i32_t));

    nkir_gen(p, nkir_make_mod(var, nkir_makeArgRef(p, 0), nkir_makeConstRef(p, nkir_makeConst(p, {&const_2, i32_t}))));
    nkir_gen(p, nkir_make_jmpnz(var, l_else));
    nkir_gen(p, nkir_make_mov(nkir_makeRetRef(p), nkir_makeConstRef(p, nkir_makeConst(p, {&const_1, i32_t}))));
    nkir_gen(p, nkir_make_jmp(l_end));
    nkir_startBlock(p, l_else, nk_cs2s("else"));
    nkir_gen(p, nkir_make_mov(nkir_makeRetRef(p), nkir_makeConstRef(p, nkir_makeConst(p, {&const_0, i32_t}))));
    nkir_startBlock(p, l_end, nk_cs2s("end"));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    i32 res = 42;
    i32 args[] = {0};

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

    auto void_t = alloct(nkt_get_void());
    auto i8_t = alloct(nkt_get_numeric(Int8));
    auto i8_ptr_t = alloct(nkt_get_ptr(i8_t));

    auto sayHello = nkir_makeFunct(p);
    auto sayHello_fn_t =
        alloct(nkt_get_fn({void_t, alloct(nkt_get_tuple(m_alloc, nullptr, 0, 0)), NkCallConv_Nk, false}));
    nkir_startFunct(sayHello, nk_cs2s("sayHello"), sayHello_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), nk_cs2s("start"));

    char const *const_str = "Hello, World!";

    auto test_print_args_t = alloct(nkt_get_tuple(m_alloc, &i8_ptr_t, 1, 0));

    auto ar_t = alloct(nkt_get_array(i8_t, std::strlen(const_str) + 1));
    auto str_t = alloct(nkt_get_ptr(ar_t));
    auto actual_args_t = alloct(nkt_get_tuple(m_alloc, &str_t, 1, 0));

    auto so = nkir_makeShObj(p, nk_cs2s(""));
    auto print_fn = nkir_makeExtSym(
        p, so, nk_cs2s("_test_print"), alloct(nkt_get_fn({void_t, test_print_args_t, NkCallConv_Cdecl, false})));

    nkir_gen(
        p,
        nkir_make_call(
            {}, nkir_makeExtSymRef(p, print_fn), nkir_makeConstRef(p, nkir_makeConst(p, {&const_str, actual_args_t}))));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    nkir_invoke({&sayHello, sayHello_fn_t}, {}, {});

    EXPECT_STREQ(s_test_print_str, const_str);
}

extern "C" NK_EXPORT u32 _test_log2(u32 x) {
    return nk_log2u32(x);
}

extern "C" NK_EXPORT void _test_fillAr(u8 *buf, u32 size) {
    for (u32 i = 0; i < size; i++) {
        buf[i] = i;
    }
}

static u32 s_test_ar_sum;
extern "C" NK_EXPORT void _test_printAr(u8 *buf, u32 size) {
    s_test_ar_sum = 0;
    for (u32 i = 0; i < size; i++) {
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

    auto u8_t = alloct(nkt_get_numeric(Uint8));
    auto u8_ptr_t = alloct(nkt_get_ptr(u8_t));
    auto u32_t = alloct(nkt_get_numeric(Uint32));
    auto void_t = alloct(nkt_get_void());

    auto so = nkir_makeShObj(p, nk_cs2s(""));

    auto log2_args_t = alloct(nkt_get_tuple(m_alloc, &u32_t, 1, 0));
    auto log2_fn = nkir_makeExtSym(
        p, so, nk_cs2s("_test_log2"), alloct(nkt_get_fn({u32_t, log2_args_t, NkCallConv_Cdecl, false})));

    nktype_t args_types[] = {u8_ptr_t, u32_t};
    auto args_t = alloct(nkt_get_tuple(m_alloc, args_types, NK_ARRAY_COUNT(args_types), sizeof(nktype_t)));

    auto fillAr_fn =
        nkir_makeExtSym(p, so, nk_cs2s("_test_fillAr"), alloct(nkt_get_fn({void_t, args_t, NkCallConv_Cdecl, false})));

    auto printAr_fn =
        nkir_makeExtSym(p, so, nk_cs2s("_test_printAr"), alloct(nkt_get_fn({void_t, args_t, NkCallConv_Cdecl, false})));

    auto test = nkir_makeFunct(p);
    auto test_fn_t = alloct(nkt_get_fn({u32_t, alloct(nkt_get_tuple(m_alloc, nullptr, 0, 0)), NkCallConv_Nk, false}));
    nkir_startFunct(test, nk_cs2s("test"), test_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), nk_cs2s("start"));

    auto getArrSize = nkir_makeFunct(p);
    auto getArrSize_fn_t =
        alloct(nkt_get_fn({u32_t, alloct(nkt_get_tuple(m_alloc, nullptr, 0, 0)), NkCallConv_Nk, false}));
    nkir_startFunct(getArrSize, nk_cs2s("getArrSize"), getArrSize_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), nk_cs2s("start"));

    u32 const_64 = 64;

    nkir_gen(
        p,
        nkir_make_call(
            nkir_makeRetRef(p),
            nkir_makeExtSymRef(p, log2_fn),
            nkir_makeConstRef(p, nkir_makeConst(p, {&const_64, log2_args_t}))));
    nkir_gen(p, nkir_make_ret());

    u64 ar_size = 0; // Must be aligned to 8
    nkir_invoke({&getArrSize, getArrSize_fn_t}, {&ar_size, u32_t}, {});
    EXPECT_EQ(ar_size, 6);

    EXPECT_NE(p, nullptr);

    nkir_activateFunct(p, test);

    auto ar_t = alloct(nkt_get_array(u8_t, ar_size));

    auto ar = nkir_makeFrameRef(p, nkir_makeLocalVar(p, ar_t));
    auto args = nkir_makeFrameRef(p, nkir_makeLocalVar(p, args_t));

    auto buf_arg = args;
    buf_arg.type = args_types[0];
    buf_arg.offset = args_t->as.tuple.elems.data[0].offset;
    auto size_arg = args;
    size_arg.type = args_types[1];
    size_arg.offset = args_t->as.tuple.elems.data[1].offset;

    nkir_gen(p, nkir_make_lea(buf_arg, ar));
    nkir_gen(p, nkir_make_mov(size_arg, nkir_makeConstRef(p, nkir_makeConst(p, {(void *)&ar_t->size, u32_t}))));
    nkir_gen(p, nkir_make_call({}, nkir_makeExtSymRef(p, fillAr_fn), args));
    nkir_gen(p, nkir_make_call({}, nkir_makeExtSymRef(p, printAr_fn), args));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    nkir_invoke({&test, test_fn_t}, {}, {});

    EXPECT_EQ(s_test_ar_sum, 15);
}

extern "C" NK_EXPORT void _test_sayHello(void *getName) {
    NkArena arena{};
    defer {
        nk_arena_free(&arena);
    };
    auto alloc = nk_arena_getAllocator(&arena);

    auto u8_t = alloct(&arena, nkt_get_numeric(Uint8));
    auto u8_ptr_t = alloct(&arena, nkt_get_ptr(u8_t));

    auto getName_fn_t = alloct(
        &arena, nkt_get_fn({u8_ptr_t, alloct(&arena, nkt_get_tuple(alloc, nullptr, 0, 0)), NkCallConv_Nk, false}));

    char const *name = nullptr;
    nkir_invoke({&getName, getName_fn_t}, {&name, u8_ptr_t}, {});

    auto buf = new char[100];
    std::snprintf(buf, 100, "Hello, %s!", name);
    _test_print(buf);
}

TEST_F(ir, callback) {
    auto p = nkir_createProgram();
    defer {
        nkir_deinitProgram(p);
    };

    auto u8_t = alloct(nkt_get_numeric(Uint8));
    auto u8_ptr_t = alloct(nkt_get_ptr(u8_t));
    auto void_t = alloct(nkt_get_void());

    auto so = nkir_makeShObj(p, nk_cs2s(""));

    auto getName_fn_t =
        alloct(nkt_get_fn({u8_ptr_t, alloct(nkt_get_tuple(m_alloc, nullptr, 0, 0)), NkCallConv_Nk, false}));

    auto args_t = alloct(nkt_get_tuple(m_alloc, &getName_fn_t, 1, 0));

    auto sayHello_fn = nkir_makeExtSym(
        p, so, nk_cs2s("_test_sayHello"), alloct(nkt_get_fn({void_t, args_t, NkCallConv_Cdecl, false})));

    auto getName = nkir_makeFunct(p);
    nkir_startFunct(getName, nk_cs2s("getName"), getName_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), nk_cs2s("start"));

    char const *const_str = "my name is ****";

    auto ar_t = alloct(nkt_get_array(u8_t, std::strlen(const_str) + 1));
    auto str_t = alloct(nkt_get_ptr(ar_t));
    auto actual_args_t = alloct(nkt_get_tuple(m_alloc, &str_t, 1, 0));

    nkir_gen(
        p, nkir_make_mov(nkir_makeRetRef(p), nkir_makeConstRef(p, nkir_makeConst(p, {&const_str, actual_args_t}))));
    nkir_gen(p, nkir_make_ret());

    auto test = nkir_makeFunct(p);
    auto test_fn_t = alloct(nkt_get_fn({void_t, alloct(nkt_get_tuple(m_alloc, nullptr, 0, 0)), NkCallConv_Nk, false}));
    nkir_startFunct(test, nk_cs2s("test"), test_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), nk_cs2s("start"));

    auto getName_arg = nkir_makeConstRef(p, nkir_makeConst(p, {&getName, getName_fn_t}));
    getName_arg.type = args_t;

    nkir_gen(p, nkir_make_call({}, nkir_makeExtSymRef(p, sayHello_fn), getName_arg));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    nkir_invoke({&test, test_fn_t}, {}, {});

    EXPECT_STREQ(s_test_print_str, "Hello, my name is ****!");
    delete[] s_test_print_str;
}

extern "C" NK_EXPORT u32 _test_nativeCallback(u32 (*cb)(u32, u32)) {
    return cb(12, 34);
}

TEST_F(ir, callback_from_native) {
    auto p = nkir_createProgram();
    defer {
        nkir_deinitProgram(p);
    };

    auto u32_t = alloct(nkt_get_numeric(Uint32));

    auto so = nkir_makeShObj(p, nk_cs2s(""));

    nktype_t nativeAdd_args_types[] = {u32_t, u32_t};
    auto nativeAdd_args_t =
        alloct(nkt_get_tuple(m_alloc, nativeAdd_args_types, NK_ARRAY_COUNT(nativeAdd_args_types), sizeof(nktype_t)));
    auto nativeAdd_fn_t = alloct(nkt_get_fn({u32_t, nativeAdd_args_t, NkCallConv_Cdecl, false}));

    auto nativeCallback_args_t = alloct(nkt_get_tuple(m_alloc, &nativeAdd_fn_t, 1, 0));

    auto test_nativeCallback_fn = nkir_makeExtSym(
        p,
        so,
        nk_cs2s("_test_nativeCallback"),
        alloct(nkt_get_fn({u32_t, nativeCallback_args_t, NkCallConv_Cdecl, false})));

    auto nativeAdd = nkir_makeFunct(p);

    nkir_startFunct(nativeAdd, nk_cs2s("nativeAdd"), nativeAdd_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), nk_cs2s("start"));

    nkir_gen(p, nkir_make_add(nkir_makeRetRef(p), nkir_makeArgRef(p, 0), nkir_makeArgRef(p, 1)));
    nkir_gen(p, nkir_make_ret());

    auto nativeAdd_cl = nkir_makeNativeClosure(p, nativeAdd);

    auto test = nkir_makeFunct(p);
    auto test_fn_t = alloct(nkt_get_fn({u32_t, alloct(nkt_get_tuple(m_alloc, nullptr, 0, 0)), NkCallConv_Nk, false}));
    nkir_startFunct(test, nk_cs2s("test"), test_fn_t);
    nkir_startBlock(p, nkir_makeBlock(p), nk_cs2s("start"));

    auto cb_arg = nkir_makeConstRef(p, nkir_makeConst(p, {nativeAdd_cl, nativeAdd_fn_t}));
    cb_arg.type = nativeCallback_args_t;

    nkir_gen(p, nkir_make_call(nkir_makeRetRef(p), nkir_makeExtSymRef(p, test_nativeCallback_fn), cb_arg));
    nkir_gen(p, nkir_make_ret());

    inspect(p);

    u64 res = 0; // Must be aligned to 8
    nkir_invoke({&test, test_fn_t}, {&res, u32_t}, {});
    EXPECT_EQ(res, 46);
}
