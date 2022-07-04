#include "nk/vm/interp.hpp"

#include <cmath>
#include <cstdint>
#include <thread>

#include <gtest/gtest.h>

#include "find_library.hpp"
#include "nk/mem/stack_allocator.hpp"
#include "nk/str/dynamic_string_builder.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
#include "nk/utils/utils.hpp"
#include "nk/vm/ir.hpp"
#include "utils/test_ir.hpp"
#include "utils/testlib.h"

namespace {

using namespace nk::vm;
using namespace nk;

LOG_USE_SCOPE(nk::vm::interp::test);

class interp : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        id_init();
        types_init();

        FindLibraryConfig conf{};
        ARRAY_SLICE_INIT(string, paths, cs2s(TESTLIB_PATH));
        conf.search_paths = paths;
        findLibrary_init(conf);

        m_ir_prog.init();
        m_prog.init();

        m_sb.reserve(1000);
    }

    void TearDown() override {
        m_sb.deinit();

        m_prog.deinit();
        m_ir_prog.deinit();
        m_arena.deinit();

        findLibrary_deinit();

        types_deinit();
        id_deinit();
    }

protected:
    ProfilerWrapper m_prof;

    StackAllocator m_arena{};
    ir::Program m_ir_prog{};
    ir::ProgramBuilder m_ir_builder{m_ir_prog};
    bc::Program m_prog{};
    bc::ProgramBuilder m_builder{m_ir_prog, m_prog};

    DynamicStringBuilder m_sb{};
};

} // namespace

TEST_F(interp, plus) {
    auto funct = test_ir_plus(m_ir_builder);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    int64_t ret = 0;
    int64_t args[] = {4, 5};
    val_fn_invoke(fn_t, {&ret, fn_t->as.fn.ret_t}, {args, fn_t->as.fn.args_t});

    EXPECT_EQ(ret, 9);
}

TEST_F(interp, not ) {
    auto funct = test_ir_not(m_ir_builder);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    int64_t ret = 42;
    int64_t arg = 1;
    val_fn_invoke(fn_t, {&ret, fn_t->as.fn.ret_t}, {&arg, fn_t->as.fn.args_t});
    EXPECT_EQ(ret, 0);

    ret = 42;
    arg = 0;
    val_fn_invoke(fn_t, {&ret, fn_t->as.fn.ret_t}, {&arg, fn_t->as.fn.args_t});
    EXPECT_EQ(ret, 1);
}

TEST_F(interp, atan) {
    auto funct = test_ir_atan(m_ir_builder);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    double a;
    double b;

    struct {
        double x;
        uint64_t it = 10;
    } args;

    args.x = 1.0 / 5.0;
    val_fn_invoke(fn_t, {&a, fn_t->as.fn.ret_t}, {&args, fn_t->as.fn.args_t});

    args.x = 1.0 / 239.0;
    val_fn_invoke(fn_t, {&b, fn_t->as.fn.ret_t}, {&args, fn_t->as.fn.args_t});

    double pi = 4.0 * (4.0 * a - b);

    EXPECT_DOUBLE_EQ(pi, 3.141592653589794);
}

TEST_F(interp, pi) {
    auto funct = test_ir_pi(m_ir_builder);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    double pi = 0;
    val_fn_invoke(fn_t, {&pi, fn_t->as.fn.ret_t}, {});

    EXPECT_DOUBLE_EQ(pi, 3.141592653589794);
}

TEST_F(interp, rsqrt) {
    auto funct = test_ir_rsqrt(m_ir_builder);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    float ret = 42;
    float arg = 2;
    val_fn_invoke(fn_t, {&ret, fn_t->as.fn.ret_t}, {&arg, fn_t->as.fn.args_t});
    EXPECT_FLOAT_EQ(ret, 1.0f / std::sqrt(2.0f));
}

TEST_F(interp, vec2LenSquared) {
    auto funct = test_ir_vec2LenSquared(m_ir_builder);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    struct Vec2 {
        double x;
        double y;
    };
    Vec2 v{4.123, 5.456};
    double len2 = 42;

    struct Args {
        Vec2 *vec_ptr;
        double *res_ptr;
    };
    Args args{&v, &len2};

    val_fn_invoke(fn_t, {}, {&args, fn_t->as.fn.args_t});

    EXPECT_DOUBLE_EQ(len2, v.x * v.x + v.y * v.y);
}

TEST_F(interp, modf) {
    auto funct = test_ir_modf(m_ir_builder);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    double fract_part = 42;
    double int_part = 42;

    struct Args {
        double x;
        double *int_part_ptr;
    };
    Args args{8.123456, &int_part};

    val_fn_invoke(fn_t, {&fract_part, fn_t->as.fn.ret_t}, {&args, fn_t->as.fn.args_t});

    EXPECT_NEAR(fract_part, 0.123456, 1e-6);
    EXPECT_NEAR(int_part, 8.0, 1e-6);
}

TEST_F(interp, intPart) {
    auto funct = test_ir_intPart(m_ir_builder);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    double arg = 123.456;
    double res = 42;

    val_fn_invoke(fn_t, {&res, fn_t->as.fn.ret_t}, {&arg, fn_t->as.fn.args_t});

    EXPECT_NEAR(res, 123.0, 1e-6);
}

static void _printThreadId(type_t, value_t, value_t) {
    LOG_INF("thread_id=%lu", std::this_thread::get_id());
}

TEST_F(interp, threads) {
    auto void_t = type_get_void();
    auto args_t = type_get_tuple({});

    auto callback = type_get_fn(void_t, args_t, 0, _printThreadId, nullptr);

    auto funct = test_ir_call3Times(m_ir_builder, callback);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    auto thread_func = [&]() {
        val_fn_invoke(fn_t, {}, {});
    };

    std::thread t0{thread_func};
    std::thread t1{thread_func};

    t0.join();
    t1.join();
}

TEST_F(interp, threads_diff_progs) {
    auto void_t = type_get_void();
    auto args_t = type_get_tuple({});

    auto callback = type_get_fn(void_t, args_t, 0, _printThreadId, nullptr);

    auto funct = test_ir_call3Times(m_ir_builder, callback);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());

    bc::ProgramBuilder &builder0 = m_builder;

    bc::Program prog1;
    prog1.init();

    bc::ProgramBuilder builder1{m_ir_prog, prog1};

    defer {
        prog1.deinit();
    };

    auto fn0_t = builder0.translate(funct);
    auto fn1_t = builder1.translate(funct);

    ASSERT_NE(fn0_t, nullptr);
    ASSERT_NE(fn1_t, nullptr);

    std::thread t0{[&]() {
        val_fn_invoke(fn0_t, {}, {});
    }};
    std::thread t1{[&]() {
        val_fn_invoke(fn1_t, {}, {});
    }};

    t0.join();
    t1.join();
}

TEST_F(interp, one_thread_diff_progs) {
    ir::Program &ir_prog0 = m_ir_prog;
    ir::ProgramBuilder &ir_builder0 = m_ir_builder;
    ir::Program ir_prog1{};
    ir_prog1.init();
    ir::ProgramBuilder ir_builder1{ir_prog1};

    bc::Program &prog0 = m_prog;
    bc::ProgramBuilder &builder0 = m_builder;
    bc::Program prog1{};
    prog1.init();
    bc::ProgramBuilder builder1{ir_prog1, prog1};

    defer {
        ir_prog1.deinit();
        prog1.deinit();
    };

    auto void_t = type_get_void();
    auto args_t = type_get_tuple({});

    auto callback = type_get_fn(void_t, args_t, 0, _printThreadId, nullptr);

    auto funct0 = test_ir_call3Times(ir_builder0, callback);
    auto fn0_t = builder0.translate(funct0);

    auto funct1 = test_ir_call3Times(ir_builder1, fn0_t);
    auto fn1_t = builder1.translate(funct1);

    auto str = ir_prog0.inspect(m_sb).moveStr(m_arena);
    LOG_INF("PROG0 ir:\n%.*s", str.size, str.data);

    str = prog0.inspect(fn0_t, m_sb).moveStr(m_arena);
    LOG_INF("PROG0 bytecode:\n\n%.*s", str.size, str.data);

    str = ir_prog1.inspect(m_sb).moveStr(m_arena);
    LOG_INF("PROG1 ir:\n%.*s", str.size, str.data);

    str = prog1.inspect(fn1_t, m_sb).moveStr(m_arena);
    LOG_INF("PROG1 bytecode:\n\n%.*s", str.size, str.data);

    val_fn_invoke(fn1_t, {}, {});
}

TEST_F(interp, hasZeroByte32) {
    auto funct = test_ir_hasZeroByte32(m_ir_builder);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    auto hasZeroByte32 = [&](int32_t x) -> bool {
        int32_t res = 42;
        val_fn_invoke(fn_t, {&res, fn_t->as.fn.ret_t}, {&x, fn_t->as.fn.args_t});
        return res;
    };

    EXPECT_TRUE(hasZeroByte32(0));
    EXPECT_FALSE(hasZeroByte32(0x01010101));
    EXPECT_FALSE(hasZeroByte32(0xffffffff));
    EXPECT_TRUE(hasZeroByte32(0xffffff00));
    EXPECT_TRUE(hasZeroByte32(0xffff00ff));
    EXPECT_TRUE(hasZeroByte32(0xff00ffff));
    EXPECT_TRUE(hasZeroByte32(0x00ffffff));
    EXPECT_FALSE(hasZeroByte32(0xf00fffff));
    EXPECT_TRUE(hasZeroByte32(0xab00cdef));
}

static void _nativeSayHello() {
    LOG_INF(__func__);
    printf("Hello, World!\n");
}

TEST_F(interp, callNativeSayHello) {
    auto funct = test_ir_callNativeFunc(m_ir_builder, (void *)_nativeSayHello);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    val_fn_invoke(fn_t, {}, {});
}

static int64_t _nativeAdd(int64_t a, int64_t b) {
    LOG_INF(__func__);
    return a + b;
}

TEST_F(interp, callNativeAdd) {
    auto funct = test_ir_callNativeAdd(m_ir_builder, (void *)_nativeAdd);
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    int64_t res = 42;
    int64_t args[] = {4, 5};
    val_fn_invoke(fn_t, {&res, fn_t->as.fn.ret_t}, {args, fn_t->as.fn.args_t});
    EXPECT_EQ(res, 9);
}

TEST_F(interp, callExternalPrintf) {
    auto funct = test_ir_callExternalPrintf(m_ir_builder, cs2s(TESTLIB_NAME));
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto fn_t = m_builder.translate(funct);
    ASSERT_NE(fn_t, nullptr);

    EXPECT_FALSE(s_test_printf_called);
    val_fn_invoke(fn_t, {}, {});
    EXPECT_TRUE(s_test_printf_called);
}

TEST_F(interp, getSetExternalVar) {
    auto [get_funct, set_funct] = test_ir_getSetExternalVar(m_ir_builder, cs2s(TESTLIB_NAME));
    LOG_INF("ir:\n%s", [&]() {
        return m_ir_prog.inspect(m_sb).moveStr(m_arena).data;
    }());
    auto get_fn_t = m_builder.translate(get_funct);
    auto set_fn_t = m_builder.translate(set_funct);
    ASSERT_NE(get_fn_t, nullptr);
    ASSERT_NE(set_fn_t, nullptr);

    auto getExternalVar = [&]() {
        int64_t res;
        val_fn_invoke(get_fn_t, {&res, get_fn_t->as.fn.ret_t}, {});
        return res;
    };

    auto setExternalVar = [&]() {
        val_fn_invoke(set_fn_t, {}, {});
    };

    EXPECT_EQ(getExternalVar(), 0);
    setExternalVar();
    EXPECT_EQ(getExternalVar(), 42);
}
