#include "nk/vm/c_compiler.hpp"

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/vm/ir.hpp"
#include "utils/test_ir.hpp"

using namespace nk::vm;

namespace {

LOG_USE_SCOPE(nk::vm::c_compiler::test)

class c_compiler : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        id_init();

        m_arena.init();
        types_init();

        m_prog.init();
        m_builder.init(m_prog);
    }

    void TearDown() override {
        m_prog.deinit();

        types_deinit();
        m_arena.deinit();

        id_deinit();
    }

protected:
    ArenaAllocator m_arena;
    ir::Program m_prog;
    ir::ProgramBuilder m_builder;

    CCompiler m_compiler;
};

} // namespace

TEST_F(c_compiler, basic) {
    ir::test_ir_main(m_builder);

    auto str = m_prog.inspect(m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    m_compiler.compile(cstr_to_str("test"), m_prog);
}
