#include "nkl/core/compiler.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.hpp"
#include "nk/vm/vm.hpp"

namespace {

using namespace nk;
using namespace nkl;

LOG_USE_SCOPE(nkl::compiler::test)

class parser : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        vm::vm_init({});
        m_ast.init();
    }

    void TearDown() override {
        m_compiler.prog.deinit();

        m_ast.deinit();
        vm::vm_deinit();
    }

protected:
    Compiler m_compiler;
    Ast m_ast;
};

} // namespace

TEST_F(parser, empty) {
    m_compiler.compile(m_ast.push(m_ast.make_block({})));
}
