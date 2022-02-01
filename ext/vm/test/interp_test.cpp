#include "nk/vm/interp.hpp"

#include <cmath>
#include <cstdint>
#include <thread>

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/profiler.hpp"
#include "nk/common/utils.hpp"
#include "nk/vm/ir.hpp"
#include "utils/ir_utils.hpp"

using namespace nk::vm;

namespace {

LOG_USE_SCOPE(nk::vm::interp::test)

class interp : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_arena.init();
        types_init();

        m_builder.init();
        m_prog = {};
    }

    void TearDown() override {
        m_prog.deinit();
        m_builder.deinit();

        types_deinit();
        m_arena.deinit();
    }

protected:
    ProfilerWrapper m_prof;

    ArenaAllocator m_arena;
    ir::ProgramBuilder m_builder;
    Program m_prog;
    Translator m_translator;
};

} // namespace

TEST_F(interp, plus) {
    buildTestIr_plus(m_builder);
    auto fn_t = m_translator.translateFromIr(m_prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

    int64_t ret = 0;
    int64_t args[] = {4, 5};
    val_fn_invoke(fn_t, {&ret, fn_t->as.fn.ret_t}, {args, fn_t->as.fn.args_t});

    EXPECT_EQ(ret, 9);
}

TEST_F(interp, not ) {
    buildTestIr_not(m_builder);
    auto fn_t = m_translator.translateFromIr(m_prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

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
    buildTestIr_atan(m_builder);
    auto fn_t = m_translator.translateFromIr(m_prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

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
    buildTestIr_pi(m_builder);
    auto fn_t = m_translator.translateFromIr(m_prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

    double pi = 0;
    val_fn_invoke(fn_t, {&pi, fn_t->as.fn.ret_t}, {nullptr, fn_t->as.fn.args_t});

    EXPECT_DOUBLE_EQ(pi, 3.141592653589794);
}

TEST_F(interp, rsqrt) {
    buildTestIr_rsqrt(m_builder);
    auto fn_t = m_translator.translateFromIr(m_prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

    float ret = 42;
    float arg = 2;
    val_fn_invoke(fn_t, {&ret, fn_t->as.fn.ret_t}, {&arg, fn_t->as.fn.args_t});
    EXPECT_FLOAT_EQ(ret, 1.0f / std::sqrt(2.0f));
}

TEST_F(interp, vec2LenSquared) {
    buildTestIr_vec2LenSquared(m_builder);
    auto fn_t = m_translator.translateFromIr(m_prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

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

    val_fn_invoke(fn_t, {nullptr, fn_t->as.fn.ret_t}, {&args, fn_t->as.fn.args_t});

    EXPECT_DOUBLE_EQ(len2, v.x * v.x + v.y * v.y);
}

TEST_F(interp, modf) {
    buildTestIr_modf(m_builder);
    auto fn_t = m_translator.translateFromIr(m_prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

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
    buildTestIr_intPart(m_builder);
    auto fn_t = m_translator.translateFromIr(m_prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

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
    auto args_t = type_get_tuple(&m_arena, {});

    auto callback = type_get_fn(void_t, args_t, 0, _printThreadId, nullptr);

    buildTestIr_call10Times(m_builder, callback);
    auto fn_t = m_translator.translateFromIr(m_prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

    auto thread_func = [&]() {
        val_fn_invoke(fn_t, {nullptr, fn_t->as.fn.ret_t}, {nullptr, fn_t->as.fn.args_t});
    };

    std::thread t0{thread_func};
    std::thread t1{thread_func};

    t0.join();
    t1.join();
}

TEST_F(interp, threads_diff_progs) {
    auto void_t = type_get_void();
    auto args_t = type_get_tuple(&m_arena, {});

    auto callback = type_get_fn(void_t, args_t, 0, _printThreadId, nullptr);

    buildTestIr_call10Times(m_builder, callback);

    Program prog{};
    DEFER({ prog.deinit(); });

    auto fn0_t = m_translator.translateFromIr(m_prog, m_builder.prog);
    auto fn1_t = m_translator.translateFromIr(prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

    std::thread t0{[&]() {
        val_fn_invoke(fn0_t, {nullptr, fn0_t->as.fn.ret_t}, {nullptr, fn0_t->as.fn.args_t});
    }};
    std::thread t1{[&]() {
        val_fn_invoke(fn1_t, {nullptr, fn1_t->as.fn.ret_t}, {nullptr, fn1_t->as.fn.args_t});
    }};

    t0.join();
    t1.join();
}

TEST_F(interp, one_thread_diff_progs) {
    ir::ProgramBuilder &builder0 = m_builder;
    ir::ProgramBuilder builder1;
    builder1.init();

    Program &prog0 = m_prog;
    Program prog1{};

    DEFER({
        builder1.deinit();
        prog1.deinit();
    })

    auto void_t = type_get_void();
    auto args_t = type_get_tuple(&m_arena, {});

    auto callback = type_get_fn(void_t, args_t, 0, _printThreadId, nullptr);

    buildTestIr_call10Times(builder0, callback);
    auto fn0_t = m_translator.translateFromIr(prog0, builder0.prog);

    buildTestIr_call10Times(builder1, fn0_t);
    auto fn1_t = m_translator.translateFromIr(prog1, builder1.prog);

    auto str = builder0.inspect(&m_arena);
    LOG_INF("PROG0 ir:\n%.*s", str.size, str.data);

    str = prog0.inspect(&m_arena);
    LOG_INF("PROG0 bytecode:\n\n%.*s", str.size, str.data);

    str = builder1.inspect(&m_arena);
    LOG_INF("PROG1 ir:\n%.*s", str.size, str.data);

    str = prog1.inspect(&m_arena);
    LOG_INF("PROG1 bytecode:\n\n%.*s", str.size, str.data);

    val_fn_invoke(fn1_t, {nullptr, fn1_t->as.fn.ret_t}, {nullptr, fn1_t->as.fn.args_t});
}
