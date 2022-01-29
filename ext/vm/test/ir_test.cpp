#include "nk/vm/ir.hpp"

#include <cstdint>

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"
#include "utils/ir_utils.hpp"

using namespace nk;
using namespace nk::vm::ir;

namespace {

LOG_USE_SCOPE(nk::vm::ir::test)

class ir : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_arena.init();
        vm::types_init();

        m_builder.init();
    }

    void TearDown() override {
        m_builder.deinit();

        vm::types_deinit();
        m_arena.deinit();
    }

protected:
    ArenaAllocator m_arena;
    ProgramBuilder m_builder;
};

} // namespace

TEST_F(ir, basic) {
    buildTestIr_basic(m_builder);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n\n%.*s", str.size, str.data);
}

TEST_F(ir, ifElse) {
    buildTestIr_ifElse(m_builder);

    auto str = m_builder.inspect(&m_arena);
    LOG_INF("ir:\n\n%.*s", str.size, str.data);
}
