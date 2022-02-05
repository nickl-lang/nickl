#include "nk/vm/ir.hpp"

#include <cstdint>

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"
#include "utils/test_ir.hpp"

using namespace nk;
using namespace nk::vm::ir;

namespace {

LOG_USE_SCOPE(nk::vm::ir::test)

class ir : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_arena.init();
        vm::types_init();

        m_prog.init();
        m_builder.init(m_prog);
    }

    void TearDown() override {
        m_prog.deinit();

        vm::types_deinit();
        m_arena.deinit();
    }

protected:
    ArenaAllocator m_arena;
    Program m_prog;
    ProgramBuilder m_builder;
};

} // namespace

//@Incomplete Add expects to IR tests

TEST_F(ir, plus) {
    test_ir_plus(m_builder);

    auto str = m_prog.inspect(m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);
}

TEST_F(ir, not ) {
    test_ir_not(m_builder);

    auto str = m_prog.inspect(m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);
}

TEST_F(ir, atan) {
    test_ir_atan(m_builder);

    auto str = m_prog.inspect(m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);
}
