#include "nkl/core/compiler.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.hpp"
#include "nk/vm/bc.hpp"
#include "nk/vm/interp.hpp"
#include "nk/vm/vm.hpp"

namespace {

using namespace nk;
using namespace nkl;

LOG_USE_SCOPE(nkl::compiler::test)

token_ref_t mkt(ETokenID id = t_eof, char const *text = "") {
    Token *token = _mctx.tmp_allocator->alloc<Token>();
    *token = Token{{text, std::strlen(text)}, 0, 0, 0, (uint8_t)id};
    return token;
}

class compiler : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        vm::VmConfig conf;
        string paths[] = {cs2s(LIBS_SEARCH_PATH)};
        conf.find_library.search_paths = {paths, sizeof(paths) / sizeof(paths[0])};
        vm_init(conf);

        m_ast.init();
    }

    void TearDown() override {
        m_compiler.prog.deinit();

        m_ast.deinit();

        vm::vm_deinit();
    }

protected:
    void test(node_ref_t root) {
        auto str = ast_inspect(root);
        LOG_INF("Test %.*s", str.size, str.data);
        bool compile_ok = m_compiler.compile(root);
        EXPECT_TRUE(compile_ok);
        if (!compile_ok) {
            LOG_ERR("Error message: %.*s", m_compiler.err.size, m_compiler.err.data)
        }

        vm::bc::Program prog;
        prog.init();
        DEFER({ prog.deinit(); })

        vm::bc::translateFromIr(prog, m_compiler.prog);
        auto fn_t = prog.funct_info[0].funct_t;
        val_fn_invoke(fn_t, {}, {});
    }

protected:
    Compiler m_compiler;
    Ast m_ast;
};

} // namespace

TEST_F(compiler, nop) {
    test(m_ast.push(m_ast.make_nop()));
}

TEST_F(compiler, empty) {
    test(m_ast.push(m_ast.make_block({})));
}

TEST_F(compiler, basic) {
    test(m_ast.push(m_ast.make_add(
        m_ast.make_numeric_int(mkt(t_int_const, "4")),
        m_ast.make_numeric_int(mkt(t_int_const, "5")))));
}

TEST_F(compiler, native_printf) {
    NamedNode printf_params[] = {NamedNode{
        .name = mkt(t_id, "fmt"), .node = m_ast.push(m_ast.make_ptr_type(m_ast.make_i8()))}};
    Node args[] = {
        m_ast.make_string_literal(mkt(t_escaped_str_const, "Hello, %s!\n")),
        m_ast.make_string_literal(mkt(t_str_const, "World"))};
    Node nodes[] = {
        m_ast.make_foreign_fn(
            mkt(t_str_const, LIBC_NAME),
            mkt(t_id, "printf"),
            {printf_params, sizeof(printf_params) / sizeof(printf_params[0])},
            m_ast.make_i32(),
            true),
        m_ast.make_call(
            m_ast.make_id(mkt(t_id, "printf")), {args, sizeof(args) / sizeof(args[0])})};
    test(m_ast.push(m_ast.make_scope(m_ast.make_block({nodes, sizeof(nodes) / sizeof(nodes[0])}))));
}
