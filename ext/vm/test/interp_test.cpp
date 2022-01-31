#include "nk/vm/interp.hpp"

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"
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
        m_translator.init();
    }

    void TearDown() override {
        m_translator.deinit();
        m_builder.deinit();

        types_deinit();
        m_arena.deinit();
    }

protected:
    ArenaAllocator m_arena;
    ir::ProgramBuilder m_builder;
    Translator m_translator;
};

} // namespace

TEST_F(interp, plus) {
    buildTestIr_plus(m_builder);
    auto fn_t = m_translator.translateFromIr(m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_translator.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

    int64_t ret = 0;
    int64_t args[] = {4, 5};
    val_fn_invoke(fn_t, {&ret, fn_t->as.fn.ret_t}, {args, fn_t->as.fn.args_t});

    EXPECT_EQ(ret, 9);
}

TEST_F(interp, not ) {
    buildTestIr_not(m_builder);
    auto fn_t = m_translator.translateFromIr(m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_translator.inspect(&m_arena);
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
    auto fn_t = m_translator.translateFromIr(m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_translator.inspect(&m_arena);
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
    auto fn_t = m_translator.translateFromIr(m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_translator.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

    double pi = 0;
    val_fn_invoke(fn_t, {&pi, fn_t->as.fn.ret_t}, {nullptr, fn_t->as.fn.args_t});

    EXPECT_DOUBLE_EQ(pi, 3.141592653589794);
}

TEST_F(interp, rsqrt) {
    buildTestIr_rsqrt(m_builder);
    auto fn_t = m_translator.translateFromIr(m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_translator.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

    float ret = 42;
    float arg = 2;
    val_fn_invoke(fn_t, {&ret, fn_t->as.fn.ret_t}, {&arg, fn_t->as.fn.args_t});
    EXPECT_FLOAT_EQ(ret, 1.0f / std::sqrt(2.0f));
}

TEST_F(interp, vec2LenSquared) {
    buildTestIr_vec2LenSquared(m_builder);
    auto fn_t = m_translator.translateFromIr(m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_translator.inspect(&m_arena);
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
    auto fn_t = m_translator.translateFromIr(m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_translator.inspect(&m_arena);
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
