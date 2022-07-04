#include "nk/vm/bc.hpp"

#include <cstdint>

#include <gtest/gtest.h>

#include "nk/mem/stack_allocator.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/utils.hpp"
#include "utils/test_ir.hpp"

namespace {

using namespace nk::vm;
using namespace nk;

LOG_USE_SCOPE(nk::vm::bc::test);

class bytecode : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_ir_prog.init();
        m_prog.init();
    }

    void TearDown() override {
        m_prog.deinit();
        m_ir_prog.deinit();
        m_arena.deinit();
    }

protected:
    StackAllocator m_arena{};
    ir::Program m_ir_prog{};
    ir::ProgramBuilder m_ir_builder{m_ir_prog};
    bc::Program m_prog{};
    bc::ProgramBuilder m_builder{m_ir_prog, m_prog};
};

} // namespace

//@Incomplete Add expects to BC test

TEST_F(bytecode, plus) {
    auto const funct = test_ir_plus(m_ir_builder);
    auto fn_t = m_builder.translate(funct);

    auto str = m_prog.inspect(fn_t, m_arena);
    LOG_INF("bc:\n%.*s", str.size, str.data);
}

TEST_F(bytecode, not ) {
    auto const funct = test_ir_not(m_ir_builder);
    auto fn_t = m_builder.translate(funct);

    auto str = m_prog.inspect(fn_t, m_arena);
    LOG_INF("bc:\n%.*s", str.size, str.data);
}

TEST_F(bytecode, atan) {
    auto const funct = test_ir_atan(m_ir_builder);
    auto fn_t = m_builder.translate(funct);

    auto str = m_prog.inspect(fn_t, m_arena);
    LOG_INF("bc:\n%.*s", str.size, str.data);
}
