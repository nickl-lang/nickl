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

        if (node->id == id_fn) {
            ss << "fn " << id_to_str(node->as.fn.sig.name).view() << "(";
            bool first = true;
            for (node_ref_t param = node->as.fn.sig.params.data;
                 param != node->as.fn.sig.params.data + node->as.fn.sig.params.size;
                 param++) {
                if (!first) {
                    ss << ", ";
                }
                ss << id_to_str(param->as.named_node.name).view() << ": "
                   << printNode(param->as.named_node.node);
                first = false;
            }
            ss << ") -> " << printNode(node->as.fn.sig.ret_type) << " { "
               << printNode(node->as.fn.body) << " }";
        } else if (node->id == id_add) {
            ss << printNode(node->as.binary.lhs) << " + " << printNode(node->as.binary.rhs);
        } else if (node->id == id_id) {
            ss << id_to_str(node->as.id.name).view();
        } else if (node->id == id_return) {
            ss << "return " << printNode(node->as.unary.arg) << ";";
        } else {
            assert(!"invalid test expression");
        }

        return ss.str();
    }

    node_ref_t makeTestTree() {
        NamedNode params[] = {
            {cstr_to_id("a"), m_ast.push(m_ast.make_id(id_u64))},
            {cstr_to_id("b"), m_ast.push(m_ast.make_id(id_u64))},
        };
        return m_ast.push(m_ast.make_fn(

            cstr_to_id("plus"),
            NamedNodeArray{2, params},
            m_ast.push(m_ast.make_id(id_void)),
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

    test(nop, id_nop);

    test(m_ast.push(m_ast.make_any()), id_any);
    test(m_ast.push(m_ast.make_bool()), id_bool);
    test(m_ast.push(m_ast.make_break()), id_break);
    test(m_ast.push(m_ast.make_continue()), id_continue);
    test(m_ast.push(m_ast.make_f32()), id_f32);
    test(m_ast.push(m_ast.make_f64()), id_f64);
    test(m_ast.push(m_ast.make_false()), id_false);
    test(m_ast.push(m_ast.make_i16()), id_i16);
    test(m_ast.push(m_ast.make_i32()), id_i32);
    test(m_ast.push(m_ast.make_i64()), id_i64);
    test(m_ast.push(m_ast.make_i8()), id_i8);
    test(m_ast.push(m_ast.make_string()), id_string);
    test(m_ast.push(m_ast.make_symbol()), id_symbol);
    test(m_ast.push(m_ast.make_true()), id_true);
    test(m_ast.push(m_ast.make_typeref()), id_typeref);
    test(m_ast.push(m_ast.make_u16()), id_u16);
    test(m_ast.push(m_ast.make_u32()), id_u32);
    test(m_ast.push(m_ast.make_u64()), id_u64);
    test(m_ast.push(m_ast.make_u8()), id_u8);
    test(m_ast.push(m_ast.make_void()), id_void);

    test_unary(m_ast.push(m_ast.make_addr(nop)), id_addr, nop);
    test_unary(m_ast.push(m_ast.make_alignof(nop)), id_alignof, nop);
    test_unary(m_ast.push(m_ast.make_assert(nop)), id_assert, nop);
    test_unary(m_ast.push(m_ast.make_compl(nop)), id_compl, nop);
    test_unary(m_ast.push(m_ast.make_dec(nop)), id_dec, nop);
    test_unary(m_ast.push(m_ast.make_deref(nop)), id_deref, nop);
    test_unary(m_ast.push(m_ast.make_inc(nop)), id_inc, nop);
    test_unary(m_ast.push(m_ast.make_not(nop)), id_not, nop);
    test_unary(m_ast.push(m_ast.make_pdec(nop)), id_pdec, nop);
    test_unary(m_ast.push(m_ast.make_pinc(nop)), id_pinc, nop);
    test_unary(m_ast.push(m_ast.make_return(nop)), id_return, nop);
    test_unary(m_ast.push(m_ast.make_sizeof(nop)), id_sizeof, nop);
    test_unary(m_ast.push(m_ast.make_typeof(nop)), id_typeof, nop);
    test_unary(m_ast.push(m_ast.make_uminus(nop)), id_uminus, nop);
    test_unary(m_ast.push(m_ast.make_uplus(nop)), id_uplus, nop);

    test_binary(m_ast.push(m_ast.make_add_assign(nop, nop)), id_add_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_add(nop, nop)), id_add, nop, nop);
    test_binary(m_ast.push(m_ast.make_and_assign(nop, nop)), id_and_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_and(nop, nop)), id_and, nop, nop);
    test_binary(m_ast.push(m_ast.make_array_type(nop, nop)), id_array_type, nop, nop);
    test_binary(m_ast.push(m_ast.make_assign(nop, nop)), id_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_bitand(nop, nop)), id_bitand, nop, nop);
    test_binary(m_ast.push(m_ast.make_bitor(nop, nop)), id_bitor, nop, nop);
    test_binary(m_ast.push(m_ast.make_cast(nop, nop)), id_cast, nop, nop);
    test_binary(m_ast.push(m_ast.make_colon_assign(nop, nop)), id_colon_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_div_assign(nop, nop)), id_div_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_div(nop, nop)), id_div, nop, nop);
    test_binary(m_ast.push(m_ast.make_eq(nop, nop)), id_eq, nop, nop);
    test_binary(m_ast.push(m_ast.make_ge(nop, nop)), id_ge, nop, nop);
    test_binary(m_ast.push(m_ast.make_gt(nop, nop)), id_gt, nop, nop);
    test_binary(m_ast.push(m_ast.make_index(nop, nop)), id_index, nop, nop);
    test_binary(m_ast.push(m_ast.make_le(nop, nop)), id_le, nop, nop);
    test_binary(m_ast.push(m_ast.make_lsh_assign(nop, nop)), id_lsh_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_lsh(nop, nop)), id_lsh, nop, nop);
    test_binary(m_ast.push(m_ast.make_lt(nop, nop)), id_lt, nop, nop);
    test_binary(m_ast.push(m_ast.make_mod_assign(nop, nop)), id_mod_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_mod(nop, nop)), id_mod, nop, nop);
    test_binary(m_ast.push(m_ast.make_mul_assign(nop, nop)), id_mul_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_mul(nop, nop)), id_mul, nop, nop);
    test_binary(m_ast.push(m_ast.make_ne(nop, nop)), id_ne, nop, nop);
    test_binary(m_ast.push(m_ast.make_or_assign(nop, nop)), id_or_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_or(nop, nop)), id_or, nop, nop);
    test_binary(m_ast.push(m_ast.make_rsh_assign(nop, nop)), id_rsh_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_rsh(nop, nop)), id_rsh, nop, nop);
    test_binary(m_ast.push(m_ast.make_sub_assign(nop, nop)), id_sub_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_sub(nop, nop)), id_sub, nop, nop);
    test_binary(m_ast.push(m_ast.make_while(nop, nop)), id_while, nop, nop);
    test_binary(m_ast.push(m_ast.make_xor_assign(nop, nop)), id_xor_assign, nop, nop);
    test_binary(m_ast.push(m_ast.make_xor(nop, nop)), id_xor, nop, nop);

    test_ternary(m_ast.push(m_ast.make_if(nop, nop, nop)), id_if, nop, nop, nop);
    test_ternary(m_ast.push(m_ast.make_ternary(nop, nop, nop)), id_ternary, nop, nop, nop);

    test_array(m_ast.push(m_ast.make_array(NodeArray{1, &n_nop})), id_array, nop);
    test_array(m_ast.push(m_ast.make_block(NodeArray{1, &n_nop})), id_block, nop);
    test_array(m_ast.push(m_ast.make_tuple_type(NodeArray{1, &n_nop})), id_tuple_type, nop);
    test_array(m_ast.push(m_ast.make_tuple(NodeArray{1, &n_nop})), id_tuple, nop);

    test_type_decl(
        m_ast.push(m_ast.make_struct(noid, NamedNodeArray{1, &nn_nop})),
        id_struct,
        noid,
        {noid, nop});
    test_type_decl(
        m_ast.push(m_ast.make_union(noid, NamedNodeArray{1, &nn_nop})),
        id_union,
        noid,
        {noid, nop});

    test_member(m_ast.push(m_ast.make_member(nop, noid)), id_member, nop, noid);
    test_member(m_ast.push(m_ast.make_offsetof(nop, noid)), id_offsetof, nop, noid);

    test_id(m_ast.push(m_ast.make_id(noid)), id_id, noid);
    test_id(m_ast.push(m_ast.make_sym(noid)), id_sym, noid);

    test(m_ast.push(m_ast.make_numeric_f64(3.14)), id_numeric_f64);
    EXPECT_EQ(m_node->as.numeric.val.f64, 3.14);

    test(m_ast.push(m_ast.make_numeric_i64(42)), id_numeric_i64);
    EXPECT_EQ(m_node->as.numeric.val.i64, 42);

    test(m_ast.push(m_ast.make_call(nop, NodeArray{1, &n_nop})), id_call);
    EXPECT_EQ(m_node->as.call.lhs, nop);
    EXPECT_EQ(m_node->as.call.args.size, 1);
    EXPECT_EQ(m_node->as.call.args.data->id, n_nop.id);

    test(m_ast.push(m_ast.make_var_decl(noid, nop, nop)), id_var_decl);
    EXPECT_EQ(m_node->as.var_decl.name, noid);
    EXPECT_EQ(m_node->as.var_decl.type, nop);
    EXPECT_EQ(m_node->as.var_decl.value, nop);

    test(m_ast.push(m_ast.make_fn(noid, NamedNodeArray{1, &nn_nop}, nop, nop)), id_fn);
    EXPECT_EQ(m_node->as.fn.sig.name, noid);
    EXPECT_EQ(m_node->as.fn.sig.params.size, 1);
    EXPECT_EQ(m_node->as.fn.sig.params.data->as.named_node.name, noid);
    EXPECT_EQ(m_node->as.fn.sig.params.data->as.named_node.node, nop);
    EXPECT_EQ(m_node->as.fn.sig.ret_type, nop);
    EXPECT_EQ(m_node->as.fn.body, nop);

    test(m_ast.push(m_ast.make_string_literal(&m_arena, cstr_to_str("hello"))), id_string_literal);
    EXPECT_EQ(m_node->as.str.val.view(), "hello");

    test(m_ast.push(m_ast.make_struct_literal(nop, NamedNodeArray{1, &nn_nop})), id_struct_literal);
    EXPECT_EQ(m_node->as.struct_literal.type, nop);
    EXPECT_EQ(m_node->as.struct_literal.fields.size, 1);
    EXPECT_EQ(m_node->as.struct_literal.fields.data->as.named_node.name, noid);
    EXPECT_EQ(m_node->as.struct_literal.fields.data->as.named_node.node, nop);

    test(m_ast.push(m_ast.make_fn_type(noid, NamedNodeArray{1, &nn_nop}, nop)), id_fn_type);
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
