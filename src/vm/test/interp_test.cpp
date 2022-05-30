#include "nk/vm/interp.hpp"

#include <cmath>
#include <cstdint>
#include <thread>

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nk/common/utils.hpp"
#include "nk/vm/ir.hpp"
#include "nk/vm/vm.hpp"
#include "utils/test_ir.hpp"
#include "utils/testlib.hpp"

using namespace nk::vm;

namespace {

LOG_USE_SCOPE(nk::vm::interp::test)

class interp : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        VmConfig conf;
        string paths[] = {cs2s(TESTLIB_PATH)};
        conf.find_library.search_paths = {paths, sizeof(paths) / sizeof(paths[0])};
        vm_init(conf);

        m_arena.init();
        m_ir_prog.init();
        m_builder.init(m_ir_prog);
        m_prog.init();
    }

    void TearDown() override {
        m_prog.deinit();
        m_ir_prog.deinit();
        m_arena.deinit();

        vm_deinit();
    }

protected:
    ProfilerWrapper m_prof;

    ArenaAllocator m_arena;
    ir::Program m_ir_prog;
    ir::ProgramBuilder m_builder;
    bc::Program m_prog;
};

} // namespace

TEST_F(interp, plus) {
    test_ir_plus(m_builder);
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[0].funct_t;

    int64_t ret = 0;
    int64_t args[] = {4, 5};
    val_fn_invoke(fn_t, {&ret, fn_t->as.fn.ret_t}, {args, fn_t->as.fn.args_t});

    EXPECT_EQ(ret, 9);
}

TEST_F(interp, not ) {
    test_ir_not(m_builder);
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[0].funct_t;

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
    test_ir_atan(m_builder);
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[0].funct_t;

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
    test_ir_pi(m_builder);
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[1].funct_t;

    double pi = 0;
    val_fn_invoke(fn_t, {&pi, fn_t->as.fn.ret_t}, {});

    EXPECT_DOUBLE_EQ(pi, 3.141592653589794);
}

TEST_F(interp, rsqrt) {
    test_ir_rsqrt(m_builder);
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[0].funct_t;

    float ret = 42;
    float arg = 2;
    val_fn_invoke(fn_t, {&ret, fn_t->as.fn.ret_t}, {&arg, fn_t->as.fn.args_t});
    EXPECT_FLOAT_EQ(ret, 1.0f / std::sqrt(2.0f));
}

TEST_F(interp, vec2LenSquared) {
    test_ir_vec2LenSquared(m_builder);
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[0].funct_t;

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
    test_ir_modf(m_builder);
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[0].funct_t;

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
    test_ir_intPart(m_builder);
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[1].funct_t;

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

    test_ir_call10Times(m_builder, callback);
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[0].funct_t;

    auto thread_func = [&]() {
        vm_enterThread();
        defer {
            vm_leaveThread();
        };

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

    test_ir_call10Times(m_builder, callback);

    bc::Program &prog0 = m_prog;
    bc::Program prog1;
    prog1.init();

    defer {
        prog1.deinit();
    };

    translateFromIr(prog0, m_ir_prog);
    auto fn0_t = prog0.funct_info[0].funct_t;
    translateFromIr(prog1, m_ir_prog);
    auto fn1_t = prog1.funct_info[0].funct_t;

    std::thread t0{[&]() {
        vm_enterThread();
        defer {
            vm_leaveThread();
        };

        val_fn_invoke(fn0_t, {}, {});
    }};
    std::thread t1{[&]() {
        vm_enterThread();
        defer {
            vm_leaveThread();
        };

        val_fn_invoke(fn1_t, {}, {});
    }};

    t0.join();
    t1.join();
}

TEST_F(interp, one_thread_diff_progs) {
    ir::Program &ir_prog0 = m_ir_prog;
    ir::Program ir_prog1;
    ir_prog1.init();

    bc::Program &prog0 = m_prog;
    bc::Program prog1;
    prog1.init();

    defer {
        ir_prog1.deinit();
        prog1.deinit();
    };

    auto void_t = type_get_void();
    auto args_t = type_get_tuple({});

    auto callback = type_get_fn(void_t, args_t, 0, _printThreadId, nullptr);

    m_builder.init(ir_prog0);
    test_ir_call10Times(m_builder, callback);
    translateFromIr(prog0, ir_prog0);
    auto fn0_t = prog0.funct_info[0].funct_t;

    m_builder.init(ir_prog1);
    test_ir_call10Times(m_builder, fn0_t);
    translateFromIr(prog1, ir_prog1);
    auto fn1_t = prog1.funct_info[0].funct_t;

    auto str = ir_prog0.inspect();
    LOG_INF("PROG0 ir:\n%.*s", str.size, str.data);

    str = prog0.inspect();
    LOG_INF("PROG0 bytecode:\n\n%.*s", str.size, str.data);

    str = ir_prog1.inspect();
    LOG_INF("PROG1 ir:\n%.*s", str.size, str.data);

    str = prog1.inspect();
    LOG_INF("PROG1 bytecode:\n\n%.*s", str.size, str.data);

    val_fn_invoke(fn1_t, {}, {});
}

TEST_F(interp, hasZeroByte32) {
    test_ir_hasZeroByte32(m_builder);
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[0].funct_t;

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
    LOG_INF(__FUNCTION__);
    printf("Hello, World!\n");
}

TEST_F(interp, callNativeSayHello) {
    test_ir_callNativeFunc(m_builder, (void *)_nativeSayHello);
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[0].funct_t;

    val_fn_invoke(fn_t, {}, {});
}

static int64_t _nativeAdd(int64_t a, int64_t b) {
    LOG_INF(__FUNCTION__);
    return a + b;
}

TEST_F(interp, callNativeAdd) {
    test_ir_callNativeAdd(m_builder, (void *)_nativeAdd);
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[0].funct_t;

    int64_t res = 42;
    int64_t args[] = {4, 5};
    val_fn_invoke(fn_t, {&res, fn_t->as.fn.ret_t}, {args, fn_t->as.fn.args_t});
    EXPECT_EQ(res, 9);
}

TEST_F(interp, callExternalPrintf) {
    test_ir_callExternalPrintf(m_builder, cs2s(TESTLIB_NAME));
    translateFromIr(m_prog, m_ir_prog);
    auto fn_t = m_prog.funct_info[0].funct_t;

    val_fn_invoke(fn_t, {}, {});

    EXPECT_TRUE(s_test_printf_called);
}

TEST_F(interp, getSetExternalVar) {
    test_ir_getSetExternalVar(m_builder, cs2s(TESTLIB_NAME));
    translateFromIr(m_prog, m_ir_prog);
    auto get_fn_t = m_prog.funct_info[0].funct_t;
    auto set_fn_t = m_prog.funct_info[1].funct_t;

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
