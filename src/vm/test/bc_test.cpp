#include "nk/vm/bc.hpp"

#include <cstdint>

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"
#include "nk/vm/vm.hpp"
#include "utils/test_ir.hpp"

using namespace nk::vm;

namespace {

class bytecode : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        vm_init({});

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
    ArenaAllocator m_arena;
    ir::Program m_ir_prog;
    ir::ProgramBuilder m_builder;
    bc::Program m_prog;
};

} // namespace

//@Incomplete Add expects to VM tests

TEST_F(bytecode, plus) {
    test_ir_plus(m_builder);
    translateFromIr(m_prog, m_ir_prog);
}

TEST_F(bytecode, not ) {
    test_ir_not(m_builder);
    translateFromIr(m_prog, m_ir_prog);
}

TEST_F(bytecode, atan) {
    test_ir_atan(m_builder);
    translateFromIr(m_prog, m_ir_prog);
}
