#include "nk/vm/bc.hpp"

#include <cstdint>

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/stack_allocator.hpp"
#include "nk/common/utils.hpp"
#include "utils/test_ir.hpp"

namespace {

using namespace nk::vm;
using namespace nk;

class bytecode : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_ir_prog.init();
        m_builder.init(m_ir_prog);
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
    ir::ProgramBuilder m_builder{};
    bc::Program m_prog{};
};

} // namespace

//@Incomplete Add expects to BC test

TEST_F(bytecode, plus) {
    test_ir_plus(m_builder);
    //@Incomplete bytecode not tested
    // translateFromIr(m_prog, m_ir_prog);
}

TEST_F(bytecode, not ) {
    test_ir_not(m_builder);
    //@Incomplete bytecode not tested
    // translateFromIr(m_prog, m_ir_prog);
}

TEST_F(bytecode, atan) {
    test_ir_atan(m_builder);
    //@Incomplete bytecode not tested
    // translateFromIr(m_prog, m_ir_prog);
}
