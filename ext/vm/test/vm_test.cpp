#include "nk/vm/vm.hpp"

#include <cstdint>

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"
#include "utils/ir_utils.hpp"

using namespace nk::vm;

namespace {

LOG_USE_SCOPE(nk::vm::test)

class vm : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_arena.init();
        types_init();

        m_builder.init();
        m_prog.init();
    }

    void TearDown() override {
        m_prog.deinit();
        m_builder.deinit();

        types_deinit();
        m_arena.deinit();
    }

protected:
    ArenaAllocator m_arena;
    ir::ProgramBuilder m_builder;
    Program m_prog;
    Translator m_translator;
};

} // namespace

//@Incomplete Add expects to VM tests

TEST_F(vm, plus) {
    buildTestIr_plus(m_builder);
    m_translator.translateFromIr(m_prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);
}

TEST_F(vm, not ) {
    buildTestIr_not(m_builder);
    m_translator.translateFromIr(m_prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);
}

TEST_F(vm, atan) {
    buildTestIr_atan(m_builder);
    m_translator.translateFromIr(m_prog, m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    str = m_prog.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);
}
