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

TEST_F(vm, plus) {
    buildTestIr_plus(m_builder);
    m_translator.translateFromIr(m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n\n%.*s", str.size, str.data);

    str = m_translator.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

    // TODO expect something
}

TEST_F(vm, not) {
    buildTestIr_not(m_builder);
    m_translator.translateFromIr(m_builder.prog);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n\n%.*s", str.size, str.data);

    str = m_translator.inspect(&m_arena);
    LOG_INF("bytecode:\n\n%.*s", str.size, str.data);

    // TODO expect something
}
