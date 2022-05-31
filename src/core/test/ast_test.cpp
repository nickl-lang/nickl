#include "nkl/core/ast.hpp"

#include <cassert>
#include <iostream>

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.h"
#include "nk/common/string_builder.hpp"
#include "nk/common/utils.hpp"
#include "nk/vm/vm.hpp"

namespace {

using namespace nk;
using namespace nkl;

LOG_USE_SCOPE(nkl::ast::test)

token_ref_t mkt(ETokenID id = t_eof, char const *text = "") {
    Token *token = _mctx.tmp_allocator->alloc<Token>();
    *token = Token{{text, std::strlen(text)}, 0, 0, 0, (uint8_t)id};
    return token;
}

class ast : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        vm::vm_init({});
        m_ast.init();
    }

    void TearDown() override {
        m_ast.deinit();
        vm::vm_deinit();
    }

protected:
    void test(Node const &node, ENodeId id) {
        LOG_INF("Test for #%s", s_ast_node_names[id]);
        auto str = ast_inspect(&node);
        LOG_INF("%.*s", str.size, str.data);
        m_node = m_ast.push(node);
        ASSERT_TRUE(m_node);
        EXPECT_EQ(id, m_node->id);
    }

    void test_unary(Node const &node, ENodeId id, Node const &arg) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.unary.arg->id, arg.id);
        }
    }

    void test_binary(Node const &node, ENodeId id, Node const &lhs, Node const &rhs) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.binary.lhs->id, lhs.id);
            EXPECT_EQ(m_node->as.binary.rhs->id, rhs.id);
        }
    }

    void test_ternary(
        Node const &node,
        ENodeId id,
        Node const &arg1,
        Node const &arg2,
        Node const &arg3) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.ternary.arg1->id, arg1.id);
            EXPECT_EQ(m_node->as.ternary.arg2->id, arg2.id);
            EXPECT_EQ(m_node->as.ternary.arg3->id, arg3.id);
        }
    }

    void test_array(Node const &node, ENodeId id, Node const &arg) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.array.nodes.size, 1);
            EXPECT_EQ(m_node->as.array.nodes.data->id, arg.id);
        }
    }

    void test_type_decl(Node const &node, ENodeId id, token_ref_t name, NamedNode const &field) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.type_decl.name, name);
            EXPECT_EQ(m_node->as.type_decl.fields.size, 1);
            EXPECT_EQ(m_node->as.type_decl.fields.data->as.named_node.name, field.name);
            EXPECT_EQ(m_node->as.type_decl.fields.data->as.named_node.node->id, field.node->id);
        }
    }

    void test_member(Node const &node, ENodeId id, Node const &lhs, token_ref_t name) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.member.lhs->id, lhs.id);
            EXPECT_EQ(std_view(m_node->as.member.name->text), std_view(name->text));
        }
    }

    void test_id(Node const &node, ENodeId id, token_ref_t name) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(std_view(m_node->as.token.val->text), std_view(name->text));
        }
    }

    void printNode(StringBuilder& sb, node_ref_t node) {
        if (node->id == Node_fn) {
            sb << "fn " << node->as.fn.sig.name->text << "(";
            bool first = true;
            for (node_ref_t param = node->as.fn.sig.params.data;
                 param != node->as.fn.sig.params.data + node->as.fn.sig.params.size;
                 param++) {
                if (!first) {
                    sb << ", ";
                }
                sb << param->as.named_node.name->text << ": ";
                printNode(sb, param->as.named_node.node);
                first = false;
            }
            sb << ") -> ";
            printNode(sb, node->as.fn.sig.ret_type);
            sb << " { ";
            printNode(sb, node->as.fn.body);
            sb << " }";
        } else if (node->id == Node_add) {
            printNode(sb, node->as.binary.lhs);
            sb << " + ";
            printNode(sb, node->as.binary.rhs);
        } else if (node->id == Node_id) {
            sb << node->as.token.val->text;
        } else if (node->id == Node_return) {
            sb << "return ";
            printNode(sb, node->as.unary.arg);
            sb << ";";
        } else if (node->id == Node_u64) {
            sb << "u64";
        } else if (node->id == Node_void) {
            sb << "void";
        } else {
            assert(!"invalid test expression");
        }
    }

    node_ref_t makeTestTree() {
        NamedNode params[] = {
            {mkt(t_id, "a"), m_ast.push(m_ast.make_u64())},
            {mkt(t_id, "b"), m_ast.push(m_ast.make_u64())},
        };
        return m_ast.push(m_ast.make_fn(
            mkt(t_id, "plus"),
            NamedNodeArray{params, 2},
            m_ast.make_void(),
            m_ast.make_return(
                m_ast.make_add(m_ast.make_id(mkt(t_id, "a")), m_ast.make_id(mkt(t_id, "b"))))));
    }

    Ast m_ast;
    node_ref_t m_node;
};

} // namespace

TEST_F(ast, nodes) {
    Node const nop = m_ast.make_nop();
    auto const noid = mkt(t_id, "noid");
    NamedNode const nn_nop{noid, m_ast.push(nop)};

#define N(TYPE, ID) test(m_ast.make_##ID(), Node_##ID);
#define U(TYPE, ID) test_unary(m_ast.make_##ID(nop), Node_##ID, nop);
#define B(TYPE, ID) test_binary(m_ast.make_##ID(nop, nop), Node_##ID, nop, nop);
#include "nkl/core/nodes.inl"

    test_ternary(m_ast.make_if(nop, nop, nop), Node_if, nop, nop, nop);
    test_ternary(m_ast.make_ternary(nop, nop, nop), Node_ternary, nop, nop, nop);

    test_array(m_ast.make_array(NodeArray{&nop, 1}), Node_array, nop);
    test_array(m_ast.make_block(NodeArray{&nop, 1}), Node_block, nop);
    test_array(m_ast.make_tuple_type(NodeArray{&nop, 1}), Node_tuple_type, nop);
    test_array(m_ast.make_tuple(NodeArray{&nop, 1}), Node_tuple, nop);

    test_type_decl(
        m_ast.make_struct(noid, NamedNodeArray{&nn_nop, 1}), Node_struct, noid, {noid, &nop});

    test_member(m_ast.make_member(nop, noid), Node_member, nop, noid);

    test_id(m_ast.make_id(noid), Node_id, noid);

    test(m_ast.make_numeric_float(mkt(t_float_const, "3.14")), Node_numeric_float);
    EXPECT_EQ(std_view(m_node->as.token.val->text), "3.14");

    test(m_ast.make_numeric_int(mkt(t_int_const, "42")), Node_numeric_int);
    EXPECT_EQ(std_view(m_node->as.token.val->text), "42");

    test(m_ast.make_call(nop, NodeArray{&nop, 1}), Node_call);
    EXPECT_EQ(m_node->as.call.lhs->id, nop.id);
    EXPECT_EQ(m_node->as.call.args.size, 1);
    EXPECT_EQ(m_node->as.call.args.data->id, nop.id);

    test(m_ast.make_var_decl(noid, nop, nop), Node_var_decl);
    EXPECT_EQ(m_node->as.var_decl.name, noid);
    EXPECT_EQ(m_node->as.var_decl.type->id, nop.id);
    EXPECT_EQ(m_node->as.var_decl.value->id, nop.id);

    test(m_ast.make_fn(noid, NamedNodeArray{&nn_nop, 1}, nop, nop), Node_fn);
    EXPECT_EQ(m_node->as.fn.sig.name, noid);
    EXPECT_EQ(m_node->as.fn.sig.params.size, 1);
    EXPECT_EQ(m_node->as.fn.sig.params.data->as.named_node.name, noid);
    EXPECT_EQ(m_node->as.fn.sig.params.data->as.named_node.node->id, nop.id);
    EXPECT_EQ(m_node->as.fn.sig.ret_type->id, nop.id);
    EXPECT_EQ(m_node->as.fn.body->id, nop.id);

    test(m_ast.make_string_literal(mkt(t_str_const, "hello")), Node_string_literal);
    EXPECT_EQ(std_view(m_node->as.token.val->text), "hello");

    test(m_ast.make_struct_literal(nop, NamedNodeArray{&nn_nop, 1}), Node_struct_literal);
    EXPECT_EQ(m_node->as.struct_literal.type->id, nop.id);
    EXPECT_EQ(m_node->as.struct_literal.fields.size, 1);
    EXPECT_EQ(m_node->as.struct_literal.fields.data->as.named_node.name, noid);
    EXPECT_EQ(m_node->as.struct_literal.fields.data->as.named_node.node->id, nop.id);
}

TEST_F(ast, to_source) {
    StringBuilder sb{};
    printNode(sb, makeTestTree());
    LibcAllocator allocator;
    auto str = sb.moveStr(allocator);
    defer {
        allocator.free((void *)str.data);
    };
    std::cout << str << std::endl;
    ASSERT_EQ(std_str(str), "fn plus(a: u64, b: u64) -> void { return a + b; }");
}

TEST_F(ast, inspect) {
    auto str = ast_inspect(makeTestTree());
    ASSERT_NE(str.size, 0);
    LOG_INF("%.*s", str.size, str.data);
}
