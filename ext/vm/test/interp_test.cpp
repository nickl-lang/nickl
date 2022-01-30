#include "nk/vm/interp.hpp"

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
    LOG_INF("ir:\n\n%.*s", str.size, str.data);

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
    LOG_INF("ir:\n\n%.*s", str.size, str.data);

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

TEST_F(interp, pi) {
    buildTestIr_atan(m_builder);
    auto fn_t = m_translator.translateFromIr(m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n\n%.*s", str.size, str.data);

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
