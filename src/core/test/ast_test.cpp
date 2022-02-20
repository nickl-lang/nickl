#include "nkl/core/ast.hpp"

#include <cassert>
#include <iostream>
#include <sstream>

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"
#include "nk/vm/vm.hpp"

namespace {

using namespace nk;
using namespace nkl;

LOG_USE_SCOPE(nkl::ast::test)

Token mkt(ETokenID id = t_eof, const char *text = "") {
    return Token{{text, std::strlen(text)}, 0, 0, 0, (uint8_t)id};
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
    void test(Node const &node, Id id) {
        LOG_INF("Test for #%s", s_ast_node_names[id]);
        auto str = ast_inspect(&node);
        LOG_INF("%.*s", str.size, str.data);
        m_node = m_ast.push(node);
        ASSERT_TRUE(m_node);
        EXPECT_EQ(id, m_node->id);
    }

    void test_unary(Node const &node, Id id, Node const &arg) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.unary.arg->id, arg.id);
        }
    }

    void test_binary(Node const &node, Id id, Node const &lhs, Node const &rhs) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.binary.lhs->id, lhs.id);
            EXPECT_EQ(m_node->as.binary.rhs->id, rhs.id);
        }
    }

    void test_ternary(
        Node const &node,
        Id id,
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

    void test_array(Node const &node, Id id, Node const &arg) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.array.nodes.size, 1);
            EXPECT_EQ(m_node->as.array.nodes.data->id, arg.id);
        }
    }

    void test_type_decl(Node const &node, Id id, Id name, NamedNode const &field) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.type_decl.name, name);
            EXPECT_EQ(m_node->as.type_decl.fields.size, 1);
            EXPECT_EQ(m_node->as.type_decl.fields.data->as.named_node.name, field.name);
            EXPECT_EQ(m_node->as.type_decl.fields.data->as.named_node.node->id, field.node->id);
        }
    }

    void test_member(Node const &node, Id id, Node const &lhs, Id name) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.member.lhs->id, lhs.id);
            EXPECT_EQ(m_node->as.member.name, name);
        }
    }

    void test_id(Node const &node, Id id, Id name) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.id.name, name);
        }
    }

    std::string printNode(node_ref_t node) {
        std::ostringstream ss;

        if (node->id == Node_fn) {
            ss << "fn " << id2s(node->as.fn.sig.name) << "(";
            bool first = true;
            for (node_ref_t param = node->as.fn.sig.params.data;
                 param != node->as.fn.sig.params.data + node->as.fn.sig.params.size;
                 param++) {
                if (!first) {
                    ss << ", ";
                }
                ss << id2s(param->as.named_node.name) << ": "
                   << printNode(param->as.named_node.node);
                first = false;
            }
            ss << ") -> " << printNode(node->as.fn.sig.ret_type) << " { "
               << printNode(node->as.fn.body) << " }";
        } else if (node->id == Node_add) {
            ss << printNode(node->as.binary.lhs) << " + " << printNode(node->as.binary.rhs);
        } else if (node->id == Node_id) {
            ss << id2s(node->as.id.name);
        } else if (node->id == Node_return) {
            ss << "return " << printNode(node->as.unary.arg) << ";";
        } else if (node->id == Node_u64) {
            ss << "u64";
        } else if (node->id == Node_void) {
            ss << "void";
        } else {
            assert(!"invalid test expression");
        }

        return ss.str();
    }

    node_ref_t makeTestTree() {
        NamedNode params[] = {
            {cs2id("a"), m_ast.push(m_ast.make_u64())},
            {cs2id("b"), m_ast.push(m_ast.make_u64())},
        };
        return m_ast.push(m_ast.make_fn(
            cs2id("plus"),
            NamedNodeArray{params, 2},
            m_ast.make_void(),
            m_ast.make_return(
                m_ast.make_add(m_ast.make_id(cs2id("a")), m_ast.make_id(cs2id("b"))))));
    }

    Ast m_ast;
    node_ref_t m_node;
};

} // namespace

TEST_F(ast, nodes) {
    Node const nop = m_ast.make_nop();
    Id const noid = cs2id("noid");
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

    test(m_ast.make_numeric_f64(3.14), Node_numeric_f64);
    EXPECT_EQ(m_node->as.numeric.val.f64, 3.14);

    test(m_ast.make_numeric_i64(42), Node_numeric_i64);
    EXPECT_EQ(m_node->as.numeric.val.i64, 42);

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

    auto str_token = mkt(t_str_const,  "hello");
    test(m_ast.make_string_literal(&str_token), Node_string_literal);
    EXPECT_EQ(std_view(m_node->as.str.val->text), "hello");

    test(m_ast.make_struct_literal(nop, NamedNodeArray{&nn_nop, 1}), Node_struct_literal);
    EXPECT_EQ(m_node->as.struct_literal.type->id, nop.id);
    EXPECT_EQ(m_node->as.struct_literal.fields.size, 1);
    EXPECT_EQ(m_node->as.struct_literal.fields.data->as.named_node.name, noid);
    EXPECT_EQ(m_node->as.struct_literal.fields.data->as.named_node.node->id, nop.id);
}

TEST_F(ast, to_source) {
    auto str = printNode(makeTestTree());
    std::cout << str << std::endl;
    ASSERT_STREQ(str.data(), "fn plus(a: u64, b: u64) -> void { return a + b; }");
}

TEST_F(ast, inspect) {
    auto str = ast_inspect(makeTestTree());
    ASSERT_NE(str.size, 0);
    LOG_INF("%.*s", str.size, str.data);
}
