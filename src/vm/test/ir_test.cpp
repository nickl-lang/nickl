#include "nk/vm/ir.hpp"

#include <gtest/gtest.h>

#include "nk/ds/arena.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/utils.hpp"
#include "utils/test_ir.hpp"

namespace {

using namespace nk;
using namespace nk::vm::ir;

LOG_USE_SCOPE(nk::vm::ir::test);

class ir : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_prog.init();
    }

    void TearDown() override {
        m_prog.deinit();
        m_arena.deinit();
    }

protected:
    StackAllocator m_arena{};
    Program m_prog{};
    ProgramBuilder m_builder{m_prog};
};

} // namespace

//@Incomplete Add expects to IR tests

TEST_F(ir, plus) {
    vm::test_ir_plus(m_builder);

    auto str = m_prog.inspect(m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);
}

TEST_F(ir, not ) {
    vm::test_ir_not(m_builder);

    auto str = m_prog.inspect(m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);
}

TEST_F(ir, atan) {
    vm::test_ir_atan(m_builder);

    auto str = m_prog.inspect(m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);
}
