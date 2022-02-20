#include "nkl/core/compiler.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.hpp"
#include "nk/vm/vm.hpp"

namespace {

using namespace nk;
using namespace nkl;

token_ref_t mkt(ETokenID id = t_eof, const char *text = "") {
    Token *token = _mctx.tmp_allocator->alloc<Token>();
    *token = Token{{text, std::strlen(text)}, 0, 0, 0, (uint8_t)id};
    return token;
}

class compiler : public testing::Test {
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

TEST_F(compiler, nop) {
    m_compiler.compile(m_ast.push(m_ast.make_nop()));
}

TEST_F(compiler, empty) {
    m_compiler.compile(m_ast.push(m_ast.make_block({})));
}

TEST_F(compiler, basic) {
    m_compiler.compile(m_ast.push(m_ast.make_add(
        m_ast.make_numeric_int(mkt(nkl::t_int_const, "4")),
        m_ast.make_numeric_int(mkt(nkl::t_int_const, "5")))));
}
