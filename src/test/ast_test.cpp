#include "nkl/core/ast.hpp"

#include <cassert>
#include <iostream>
#include <sstream>

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"

namespace {

LOG_USE_SCOPE(test)

class ast : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_arena.init();
        id_init();
        m_ast.init();
    }

    void TearDown() override {
        m_ast.deinit();
        id_deinit();
        m_arena.deinit();
    }

protected:
    void test(node_ref_t node, Id id) {
        string str = id_to_str(id);
        LOG_INF("Test for #%.*s", str.size, str.data);
        str = ast_inspect(&m_arena, node);
        LOG_INF("%.*s", str.size, str.data);
        m_node = node;
        ASSERT_TRUE(m_node);
        EXPECT_EQ(id, m_node->id);
    }

    void test_unary(node_ref_t node, Id id, node_ref_t arg) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.unary.arg, arg);
        }
    }

    void test_binary(node_ref_t node, Id id, node_ref_t lhs, node_ref_t rhs) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.binary.lhs, lhs);
            EXPECT_EQ(m_node->as.binary.rhs, rhs);
        }
    }

    void test_ternary(node_ref_t node, Id id, node_ref_t arg1, node_ref_t arg2, node_ref_t arg3) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.ternary.arg1, arg1);
            EXPECT_EQ(m_node->as.ternary.arg2, arg2);
            EXPECT_EQ(m_node->as.ternary.arg3, arg3);
        }
    }

    void test_array(node_ref_t node, Id id, node_ref_t arg) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.array.nodes.size, 1);
            EXPECT_EQ(m_node->as.array.nodes.data->id, arg->id);
        }
    }

    void test_type_decl(node_ref_t node, Id id, Id name, NamedNode const &field) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.type_decl.name, name);
            EXPECT_EQ(m_node->as.type_decl.fields.size, 1);
            EXPECT_EQ(m_node->as.type_decl.fields.data->as.named_node.name, field.name);
            EXPECT_EQ(m_node->as.type_decl.fields.data->as.named_node.node, field.node);
        }
    }

    void test_member(node_ref_t node, Id id, node_ref_t lhs, Id name) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.member.lhs, lhs);
            EXPECT_EQ(m_node->as.member.name, name);
        }
    }

    void test_id(node_ref_t node, Id id, Id name) {
        test(node, id);
        if (m_node) {
            EXPECT_EQ(m_node->as.id.name, name);
        }
    }

    std::string printNode(node_ref_t node) {
        std::ostringstream ss;

        if (node->id == Node_fn) {
            ss << "fn " << id_to_str(node->as.fn.sig.name) << "(";
            bool first = true;
            for (node_ref_t param = node->as.fn.sig.params.data;
                 param != node->as.fn.sig.params.data + node->as.fn.sig.params.size;
                 param++) {
                if (!first) {
                    ss << ", ";
                }
                ss << id_to_str(param->as.named_node.name) << ": "
                   << printNode(param->as.named_node.node);
                first = false;
            }
            ss << ") -> " << printNode(node->as.fn.sig.ret_type) << " { "
               << printNode(node->as.fn.body) << " }";
        } else if (node->id == Node_add) {
            ss << printNode(node->as.binary.lhs) << " + " << printNode(node->as.binary.rhs);
        } else if (node->id == Node_id) {
            ss << id_to_str(node->as.id.name);
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
            {cstr_to_id("a"), m_ast.push(m_ast.make_u64())},
            {cstr_to_id("b"), m_ast.push(m_ast.make_u64())},
        };
        return m_ast.push(m_ast.make_fn(
            cstr_to_id("plus"),
            NamedNodeArray{params, 2},
            m_ast.push(m_ast.make_void()),
            m_ast.push(m_ast.make_return(m_ast.push(m_ast.make_add(
                m_ast.push(m_ast.make_id(cstr_to_id("a"))),
                m_ast.push(m_ast.make_id(cstr_to_id("b")))))))));
    }

    ArenaAllocator m_arena;
    Ast m_ast;
    node_ref_t m_node;
};

} // namespace

TEST_F(ast, nodes) {
    Node const n_nop = m_ast.make_nop();
    node_ref_t const nop = m_ast.push(n_nop);
    Id const noid = cstr_to_id("noid");
    NamedNode const nn_nop{noid, nop};

    test(nop, Node_nop);

    test(m_ast.push(m_ast.make_any()), Node_any);
    test(m_ast.push(m_ast.make_bool()), Node_bool);
    test(m_ast.push(m_ast.make_break()), Node_break);
    test(m_ast.push(m_ast.make_continue()), Node_continue);
    test(m_ast.push(m_ast.make_f32()), Node_f32);
    test(m_ast.push(m_ast.make_f64()), Node_f64);
    test(m_ast.push(m_ast.make_false()), Node_false);
    test(m_ast.push(m_ast.make_i16()), Node_i16);
    test(m_ast.push(m_ast.make_i32()), Node_i32);
    test(m_ast.push(m_ast.make_i64()), Node_i64);
    test(m_ast.push(m_ast.make_i8()), Node_i8);
    test(m_ast.push(m_ast.make_string()), Node_string);
    test(m_ast.push(m_ast.make_symbol()), Node_symbol);
    test(m_ast.push(m_ast.make_true()), Node_true);
    test(m_ast.push(m_ast.make_typeref()), Node_typeref);
    test(m_ast.push(m_ast.make_u16()), Node_u16);
    test(m_ast.push(m_ast.make_u32()), Node_u32);
    test(m_ast.push(m_ast.make_u64()), Node_u64);
    test(m_ast.push(m_ast.make_u8()), Node_u8);
    test(m_ast.push(m_ast.make_void()), Node_void);

    test_unary(m_ast.push(m_ast.make_addr(nop)), Node_addr, nop);
    test_unary(m_ast.push(m_ast.make_alignof(nop)), Node_alignof, nop);
    test_unary(m_ast.push(m_ast.make_assert(nop)), Node_assert, nop);
    test_unary(m_ast.push(m_ast.make_compl(nop)), Node_compl, nop);
    test_unary(m_ast.push(m_ast.make_dec(nop)), Node_dec, nop);
    test_unary(m_ast.push(m_ast.make_deref(nop)), Node_deref, nop);
    test_unary(m_ast.push(m_ast.make_inc(nop)), Node_inc, nop);
    test_unary(m_ast.push(m_ast.make_not(nop)), Node_not, nop);
    test_unary(m_ast.push(m_ast.make_pdec(nop)), Node_pdec, nop);
    test_unary(m_ast.push(m_ast.make_pinc(nop)), Node_pinc, nop);
    test_unary(m_ast.push(m_ast.make_return(nop)), Node_return, nop);
    test_unary(m_ast.push(m_ast.make_sizeof(nop)), Node_sizeof, nop);
    test_unary(m_ast.push(m_ast.make_typeof(nop)), Node_typeof, nop);
    test_unary(m_ast.push(m_ast.make_uminus(nop)), Node_uminus, nop);
    test_unary(m_ast.push(m_ast.make_uplus(nop)), Node_uplus, nop);

    test_binary(m_ast.push(m_ast.make_add_assign(nop, nop)), Node_add_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_add(nop, nop)), Node_add, nop, nop);
    test_binary(m_ast.push(m_ast.make_and_assign(nop, nop)), Node_and_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_and(nop, nop)), Node_and, nop, nop);
    test_binary(m_ast.push(m_ast.make_array_type(nop, nop)), Node_array_type, nop, nop);
    test_binary(m_ast.push(m_ast.make_assign(nop, nop)), Node_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_bitand(nop, nop)), Node_bitand, nop, nop);
    test_binary(m_ast.push(m_ast.make_bitor(nop, nop)), Node_bitor, nop, nop);
    test_binary(m_ast.push(m_ast.make_cast(nop, nop)), Node_cast, nop, nop);
    test_binary(m_ast.push(m_ast.make_colon_assign(nop, nop)), Node_colon_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_div_assign(nop, nop)), Node_div_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_div(nop, nop)), Node_div, nop, nop);
    test_binary(m_ast.push(m_ast.make_eq(nop, nop)), Node_eq, nop, nop);
    test_binary(m_ast.push(m_ast.make_ge(nop, nop)), Node_ge, nop, nop);
    test_binary(m_ast.push(m_ast.make_gt(nop, nop)), Node_gt, nop, nop);
    test_binary(m_ast.push(m_ast.make_index(nop, nop)), Node_index, nop, nop);
    test_binary(m_ast.push(m_ast.make_le(nop, nop)), Node_le, nop, nop);
    test_binary(m_ast.push(m_ast.make_lsh_assign(nop, nop)), Node_lsh_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_lsh(nop, nop)), Node_lsh, nop, nop);
    test_binary(m_ast.push(m_ast.make_lt(nop, nop)), Node_lt, nop, nop);
    test_binary(m_ast.push(m_ast.make_mod_assign(nop, nop)), Node_mod_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_mod(nop, nop)), Node_mod, nop, nop);
    test_binary(m_ast.push(m_ast.make_mul_assign(nop, nop)), Node_mul_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_mul(nop, nop)), Node_mul, nop, nop);
    test_binary(m_ast.push(m_ast.make_ne(nop, nop)), Node_ne, nop, nop);
    test_binary(m_ast.push(m_ast.make_or_assign(nop, nop)), Node_or_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_or(nop, nop)), Node_or, nop, nop);
    test_binary(m_ast.push(m_ast.make_rsh_assign(nop, nop)), Node_rsh_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_rsh(nop, nop)), Node_rsh, nop, nop);
    test_binary(m_ast.push(m_ast.make_sub_assign(nop, nop)), Node_sub_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_sub(nop, nop)), Node_sub, nop, nop);
    test_binary(m_ast.push(m_ast.make_while(nop, nop)), Node_while, nop, nop);
    test_binary(m_ast.push(m_ast.make_xor_assign(nop, nop)), Node_xor_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_xor(nop, nop)), Node_xor, nop, nop);

    test_ternary(m_ast.push(m_ast.make_if(nop, nop, nop)), Node_if, nop, nop, nop);
    test_ternary(m_ast.push(m_ast.make_ternary(nop, nop, nop)), Node_ternary, nop, nop, nop);

    test_array(m_ast.push(m_ast.make_array(NodeArray{&n_nop, 1})), Node_array, nop);
    test_array(m_ast.push(m_ast.make_block(NodeArray{&n_nop, 1})), Node_block, nop);
    test_array(m_ast.push(m_ast.make_tuple_type(NodeArray{&n_nop, 1})), Node_tuple_type, nop);
    test_array(m_ast.push(m_ast.make_tuple(NodeArray{&n_nop, 1})), Node_tuple, nop);

    test_type_decl(
        m_ast.push(m_ast.make_struct(noid, NamedNodeArray{&nn_nop, 1})),
        Node_struct,
        noid,
        {noid, nop});
    test_type_decl(
        m_ast.push(m_ast.make_union(noid, NamedNodeArray{&nn_nop, 1})),
        Node_union,
        noid,
        {noid, nop});

    test_member(m_ast.push(m_ast.make_member(nop, noid)), Node_member, nop, noid);
    test_member(m_ast.push(m_ast.make_offsetof(nop, noid)), Node_offsetof, nop, noid);

    test_id(m_ast.push(m_ast.make_id(noid)), Node_id, noid);
    test_id(m_ast.push(m_ast.make_sym(noid)), Node_sym, noid);

    test(m_ast.push(m_ast.make_numeric_f64(3.14)), Node_numeric_f64);
    EXPECT_EQ(m_node->as.numeric.val.f64, 3.14);

    test(m_ast.push(m_ast.make_numeric_i64(42)), Node_numeric_i64);
    EXPECT_EQ(m_node->as.numeric.val.i64, 42);

    test(m_ast.push(m_ast.make_call(nop, NodeArray{&n_nop, 1})), Node_call);
    EXPECT_EQ(m_node->as.call.lhs, nop);
    EXPECT_EQ(m_node->as.call.args.size, 1);
    EXPECT_EQ(m_node->as.call.args.data->id, n_nop.id);

    test(m_ast.push(m_ast.make_var_decl(noid, nop, nop)), Node_var_decl);
    EXPECT_EQ(m_node->as.var_decl.name, noid);
    EXPECT_EQ(m_node->as.var_decl.type, nop);
    EXPECT_EQ(m_node->as.var_decl.value, nop);

    test(m_ast.push(m_ast.make_fn(noid, NamedNodeArray{&nn_nop, 1}, nop, nop)), Node_fn);
    EXPECT_EQ(m_node->as.fn.sig.name, noid);
    EXPECT_EQ(m_node->as.fn.sig.params.size, 1);
    EXPECT_EQ(m_node->as.fn.sig.params.data->as.named_node.name, noid);
    EXPECT_EQ(m_node->as.fn.sig.params.data->as.named_node.node, nop);
    EXPECT_EQ(m_node->as.fn.sig.ret_type, nop);
    EXPECT_EQ(m_node->as.fn.body, nop);

    test(
        m_ast.push(m_ast.make_string_literal(&m_arena, cstr_to_str("hello"))), Node_string_literal);
    EXPECT_EQ(m_node->as.str.val.view(), "hello");

    test(
        m_ast.push(m_ast.make_struct_literal(nop, NamedNodeArray{&nn_nop, 1})),
        Node_struct_literal);
    EXPECT_EQ(m_node->as.struct_literal.type, nop);
    EXPECT_EQ(m_node->as.struct_literal.fields.size, 1);
    EXPECT_EQ(m_node->as.struct_literal.fields.data->as.named_node.name, noid);
    EXPECT_EQ(m_node->as.struct_literal.fields.data->as.named_node.node, nop);

    test(m_ast.push(m_ast.make_fn_type(noid, NamedNodeArray{&nn_nop, 1}, nop)), Node_fn_type);
    EXPECT_EQ(m_node->as.fn_type.name, noid);
    EXPECT_EQ(m_node->as.fn_type.params.size, 1);
    EXPECT_EQ(m_node->as.fn_type.params.data->as.named_node.name, noid);
    EXPECT_EQ(m_node->as.fn_type.params.data->as.named_node.node, nop);
}

TEST_F(ast, to_source) {
    auto str = printNode(makeTestTree());
    std::cout << str << std::endl;
    ASSERT_STREQ(str.data(), "fn plus(a: u64, b: u64) -> void { return a + b; }");
}

TEST_F(ast, inspect) {
    auto str = ast_inspect(&m_arena, makeTestTree());
    ASSERT_NE(str.size, 0);
    LOG_INF("%.*s", str.size, str.data);
}
