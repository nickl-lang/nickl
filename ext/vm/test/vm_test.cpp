#include "nk/vm/vm.hpp"

#include <cstdint>

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"
#include "utils/ir_utils.hpp"

using namespace nk::vm;

namespace {

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

TEST_F(vm, basic) {
    buildTestIr_basic(m_builder);
    m_translator.translateFromIr(m_builder.prog);
}

TEST_F(vm, ifElse) {
    buildTestIr_ifElse(m_builder);
    m_translator.translateFromIr(m_builder.prog);
}
