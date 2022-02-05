#include "nk/vm/bc.hpp"

#include <cstdint>

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"
#include "utils/test_ir.hpp"

using namespace nk::vm;

namespace {

LOG_USE_SCOPE(nk::vm::bc::test)

class bytecode : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_arena.init();
        types_init();

        m_ir_prog.init();
        m_builder.init(m_ir_prog);

        m_prog.init();
    }

    void TearDown() override {
        m_prog.deinit();

        m_ir_prog.deinit();

        types_deinit();
        m_arena.deinit();
    }

protected:
    ArenaAllocator m_arena;
    ir::Program m_ir_prog;
    ir::ProgramBuilder m_builder;
    bc::Program m_prog;
    bc::Translator m_translator;
};

} // namespace

//@Incomplete Add expects to VM tests

TEST_F(bytecode, plus) {
    test_ir_plus(m_builder);
    m_translator.translateFromIr(m_prog, m_ir_prog);

    auto str = m_ir_prog.inspect(m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);
}

TEST_F(bytecode, not ) {
    test_ir_not(m_builder);
    m_translator.translateFromIr(m_prog, m_ir_prog);

    auto str = m_ir_prog.inspect(m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);
}

TEST_F(bytecode, atan) {
    test_ir_atan(m_builder);
    m_translator.translateFromIr(m_prog, m_ir_prog);

    auto str = m_ir_prog.inspect(m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);
}
