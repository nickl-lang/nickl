#include "translate_to_c.hpp"

#include <gtest/gtest.h>

#include "find_library.hpp"
#include "nk/str/dynamic_string_builder.hpp"
#include "nk/utils/logger.h"
#include "nk/vm/bc.hpp"
#include "nk/vm/ir.hpp"
#include "utils/test_ir.hpp"

namespace {

using namespace nk::vm;
using namespace nk;

LOG_USE_SCOPE(nk::vm::test::translate_to_c);

class translate_to_c : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_prog.init();
    }

    void TearDown() override {
        m_arena.deinit();
        m_sb.deinit();

        m_prog.deinit();
    }

protected:
    ir::Program m_prog;
    ir::ProgramBuilder m_builder{m_prog};

    DynamicStringBuilder m_sb{};
    StackAllocator m_arena{};
};

} // namespace

//@Incomplete Add expects to translateToC tests

TEST_F(translate_to_c, basic) {
    test_ir_main_argc(m_builder);
    LOG_INF("ir:\n%s", [&]() {
        return m_prog.inspect(m_sb).moveStr(m_arena).data;
    }());

    translateToC(m_prog, std::cout);
}

TEST_F(translate_to_c, pi) {
    test_ir_main_pi(m_builder, cs2s(LIBC_NAME));
    LOG_INF("ir:\n%s", [&]() {
        return m_prog.inspect(m_sb).moveStr(m_arena).data;
    }());

    translateToC(m_prog, std::cout);
}

TEST_F(translate_to_c, vec2LenSquared) {
    test_ir_main_vec2LenSquared(m_builder, cs2s(LIBC_NAME));
    LOG_INF("ir:\n%s", [&]() {
        return m_prog.inspect(m_sb).moveStr(m_arena).data;
    }());

    translateToC(m_prog, std::cout);
}
