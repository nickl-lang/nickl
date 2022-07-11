#include "nkl/lang/ast.hpp"

#include <gtest/gtest.h>

#include "nk/mem/stack_allocator.hpp"
#include "nk/str/dynamic_string_builder.hpp"
#include "nk/utils/logger.h"
#include "nkl/core/test/ast_utils.hpp"

using namespace nkl;
using namespace nkl::test;

LOG_USE_SCOPE(test::ast);

struct ast : testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_ast.init();

        id_init();

        m_sb.reserve(1000);

        t_hello = Token{cs2s("hello"), 0, 0, 0};
        n_t_hello = Node{{{&t_hello, {}}, {}, {}}, nodeId(Node_id)};

        t_x = Token{cs2s("x"), 0, 0, 0};
        t_y = Token{cs2s("y"), 0, 0, 0};
        t_z = Token{cs2s("z"), 0, 0, 0};
        t_w = Token{cs2s("w"), 0, 0, 0};

        n_t_x = Node{{{&t_x, {}}, {}, {}}, nodeId(Node_id)};
        n_t_y = Node{{{&t_y, {}}, {}, {}}, nodeId(Node_id)};
        n_t_z = Node{{{&t_z, {}}, {}, {}}, nodeId(Node_id)};
        n_t_w = Node{{{&t_w, {}}, {}, {}}, nodeId(Node_id)};
    }

    void TearDown() override {
        id_deinit();

        m_ast.deinit();
        m_allocator.deinit();

        m_sb.deinit();
    }

    void test(Node const &node) {
        m_node = node;

        auto str = ast_inspect(&m_node, m_sb).moveStr(m_allocator);
        LOG_INF("Test: %.*s", str.size, str.data);
    }

    bool expect_ast(NodeRef actual, NodeRef expected) {
        bool is_ast_equal = ast_equal(actual, expected);
        if (!is_ast_equal) {
            auto actual_str = ast_inspect(actual, m_sb).moveStr(m_allocator);
            auto expected_str = ast_inspect(expected, m_sb).moveStr(m_allocator);
            LOG_ERR(
                "\nActual: %.*s\nExpected: %.*s",
                actual_str.size,
                actual_str.data,
                expected_str.size,
                expected_str.data);
        }
        return is_ast_equal;
    }

    Token t_hello{};
    Node n_t_hello{};

    Token t_x{};
    Token t_y{};
    Token t_z{};
    Token t_w{};

    Node n_t_x{};
    Node n_t_y{};
    Node n_t_z{};
    Node n_t_w{};

    LangAst m_ast{};
    StackAllocator m_allocator{};
    DynamicStringBuilder m_sb{};

    Node m_node{};
};

#define EXPECT_AST(ACTUAL, EXPECTED)                                   \
    {                                                                  \
        bool const is_ast_equal = expect_ast((ACTUAL), (EXPECTED));    \
        EXPECT_TRUE(is_ast_equal) << "Actual expr: " << #ACTUAL        \
                                  << "\nExpected expr: " << #EXPECTED; \
    }

#define EXPECT_AST_AR(ACTUAL, EXPECTED)                 \
    {                                                   \
        auto const &actual_ar = (ACTUAL);               \
        auto const &expected_ar = (EXPECTED);           \
        EXPECT_EQ(actual_ar.size, expected_ar.size);    \
        for (size_t i = 0; i < actual_ar.size; i++) {   \
            EXPECT_AST(&actual_ar[i], &expected_ar[i]); \
        }                                               \
    }

#define EXPECT_AST_NN_AR(ACTUAL, EXPECTED)                      \
    {                                                           \
        auto const &actual_ar = (ACTUAL);                       \
        auto const &expected_ar = (EXPECTED);                   \
        EXPECT_EQ(actual_ar.size, expected_ar.size);            \
        for (size_t i = 0; i < actual_ar.size; i++) {           \
            EXPECT_EQ(actual_ar[i].name, expected_ar[i].name);  \
            EXPECT_AST(actual_ar[i].node, expected_ar[i].node); \
        }                                                       \
    }

#define EXPECT_AST_TOKEN_AR(ACTUAL, EXPECTED)         \
    {                                                 \
        auto const &actual_ar = (ACTUAL);             \
        auto const &expected_ar = (EXPECTED);         \
        EXPECT_EQ(actual_ar.size, expected_ar.size);  \
        for (size_t i = 0; i < actual_ar.size; i++) { \
            EXPECT_EQ(actual_ar[i], expected_ar[i]);  \
        }                                             \
    }

#define EXPECT_AST_FIELD_AR(ACTUAL, EXPECTED)                               \
    {                                                                       \
        auto const &actual_ar = (ACTUAL);                                   \
        auto const &expected_ar = (EXPECTED);                               \
        EXPECT_EQ(actual_ar.size, expected_ar.size);                        \
        for (size_t i = 0; i < actual_ar.size; i++) {                       \
            EXPECT_EQ(actual_ar[i].name, expected_ar[i].name);              \
            EXPECT_AST(actual_ar[i].type, expected_ar[i].type);             \
            EXPECT_AST(actual_ar[i].init_value, expected_ar[i].init_value); \
            EXPECT_EQ(actual_ar[i].is_const, expected_ar[i].is_const);      \
        }                                                                   \
    }

TEST_F(ast, nullary) {
#define N(ID) test(m_ast.CAT(make_, ID)());
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, unary) {
#define U(ID)                                     \
    test(m_ast.CAT(make_, ID)(n_t_x));            \
    EXPECT_EQ(nodeId(CAT(Node_, ID)), m_node.id); \
    EXPECT_AST(Node_unary_arg(&m_node), &n_t_x);
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, binary) {
#define B(ID)                                     \
    test(m_ast.CAT(make_, ID)(n_t_x, n_t_y));     \
    EXPECT_EQ(nodeId(CAT(Node_, ID)), m_node.id); \
    EXPECT_AST(Node_binary_lhs(&m_node), &n_t_x); \
    EXPECT_AST(Node_binary_rhs(&m_node), &n_t_y);
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, ternary) {
    test(m_ast.make_if(n_t_x, n_t_y, n_t_z));
    EXPECT_EQ(nodeId(Node_if), m_node.id);
    EXPECT_AST(Node_ternary_cond(&m_node), &n_t_x);
    EXPECT_AST(Node_ternary_then_clause(&m_node), &n_t_y);
    EXPECT_AST(Node_ternary_else_clause(&m_node), &n_t_z);

    test(m_ast.make_ternary(n_t_x, n_t_y, n_t_z));
    EXPECT_EQ(nodeId(Node_ternary), m_node.id);
    EXPECT_AST(Node_ternary_cond(&m_node), &n_t_x);
    EXPECT_AST(Node_ternary_then_clause(&m_node), &n_t_y);
    EXPECT_AST(Node_ternary_else_clause(&m_node), &n_t_z);
}

TEST_F(ast, array) {
    ARRAY_SLICE_INIT(Node const, nodes, n_t_x, n_t_y, n_t_z);

    test(m_ast.make_array(nodes));
    EXPECT_EQ(nodeId(Node_array), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes);

    test(m_ast.make_block(nodes));
    EXPECT_EQ(nodeId(Node_block), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes);

    test(m_ast.make_tuple(nodes));
    EXPECT_EQ(nodeId(Node_tuple), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes);

    test(m_ast.make_tuple_type(nodes));
    EXPECT_EQ(nodeId(Node_tuple_type), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes);

    test(m_ast.make_run(nodes));
    EXPECT_EQ(nodeId(Node_run), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes);
}

TEST_F(ast, token) {
    test(m_ast.make_id(&t_hello));
    EXPECT_EQ(nodeId(Node_id), m_node.id);
    EXPECT_EQ(Node_token_value(&m_node), &t_hello);

    test(m_ast.make_numeric_float(&t_hello));
    EXPECT_EQ(nodeId(Node_numeric_float), m_node.id);
    EXPECT_EQ(Node_token_value(&m_node), &t_hello);

    test(m_ast.make_numeric_int(&t_hello));
    EXPECT_EQ(nodeId(Node_numeric_int), m_node.id);
    EXPECT_EQ(Node_token_value(&m_node), &t_hello);

    test(m_ast.make_string_literal(&t_hello));
    EXPECT_EQ(nodeId(Node_string_literal), m_node.id);
    EXPECT_EQ(Node_token_value(&m_node), &t_hello);

    test(m_ast.make_escaped_string_literal(&t_hello));
    EXPECT_EQ(nodeId(Node_escaped_string_literal), m_node.id);
    EXPECT_EQ(Node_token_value(&m_node), &t_hello);

    test(m_ast.make_import_path(&t_hello));
    EXPECT_EQ(nodeId(Node_import_path), m_node.id);
    EXPECT_EQ(Node_token_value(&m_node), &t_hello);

    test(m_ast.make_typename(&t_hello));
    EXPECT_EQ(nodeId(Node_typename), m_node.id);
    EXPECT_EQ(Node_token_value(&m_node), &t_hello);
}

TEST_F(ast, other) {
    NamedNode nn_x{&t_x, &n_t_hello};
    NamedNode nn_y{&t_y, &n_t_hello};
    NamedNode nn_z{&t_z, &n_t_hello};
    NamedNode nn_w{&t_w, &n_t_hello};

    FieldNode f_x{&t_x, &n_t_hello, nullptr, false};
    FieldNode f_y{&t_y, &n_t_hello, nullptr, false};

    ARRAY_SLICE_INIT(Node const, nodes, n_t_x, n_t_y, n_t_z);
    ARRAY_SLICE_INIT(NamedNode const, args, nn_x, nn_y, nn_z, nn_w);
    ARRAY_SLICE_INIT(TokenRef const, tokens, &t_hello, &t_hello);
    ARRAY_SLICE_INIT(FieldNode const, fields, f_x, f_y);

    test(m_ast.make_import(tokens));
    EXPECT_EQ(nodeId(Node_import), m_node.id);
    EXPECT_AST_TOKEN_AR(Node_import_names(&m_node), tokens);

    test(m_ast.make_for(&t_hello, n_t_hello, n_t_hello));
    EXPECT_EQ(nodeId(Node_for), m_node.id);
    EXPECT_EQ(Node_for_it(&m_node), &t_hello);
    EXPECT_AST(Node_for_range(&m_node), &n_t_hello);
    EXPECT_AST(Node_for_body(&m_node), &n_t_hello);

    test(m_ast.make_for(&t_hello, n_t_hello, n_t_hello));
    EXPECT_EQ(nodeId(Node_for), m_node.id);
    EXPECT_EQ(Node_for_it(&m_node), &t_hello);
    EXPECT_AST(Node_for_range(&m_node), &n_t_hello);
    EXPECT_AST(Node_for_body(&m_node), &n_t_hello);

    test(m_ast.make_for_by_ptr(&t_hello, n_t_hello, n_t_hello));
    EXPECT_EQ(nodeId(Node_for_by_ptr), m_node.id);
    EXPECT_EQ(Node_for_it(&m_node), &t_hello);
    EXPECT_AST(Node_for_range(&m_node), &n_t_hello);
    EXPECT_AST(Node_for_body(&m_node), &n_t_hello);

    test(m_ast.make_member(n_t_hello, &t_hello));
    EXPECT_EQ(nodeId(Node_member), m_node.id);
    EXPECT_AST(Node_member_lhs(&m_node), &n_t_hello);
    EXPECT_EQ(Node_member_name(&m_node), &t_hello);

    test(m_ast.make_struct(fields));
    EXPECT_EQ(nodeId(Node_struct), m_node.id);
    EXPECT_AST_FIELD_AR(Node_type_fields(&m_node), fields);

    test(m_ast.make_union(fields));
    EXPECT_EQ(nodeId(Node_union), m_node.id);
    EXPECT_AST_FIELD_AR(Node_type_fields(&m_node), fields);

    test(m_ast.make_enum(fields));
    EXPECT_EQ(nodeId(Node_enum), m_node.id);
    EXPECT_AST_FIELD_AR(Node_type_fields(&m_node), fields);

    test(m_ast.make_packed_struct(fields));
    EXPECT_EQ(nodeId(Node_packed_struct), m_node.id);
    EXPECT_AST_FIELD_AR(Node_type_fields(&m_node), fields);

    test(m_ast.make_fn(fields, n_t_hello, n_t_hello));
    EXPECT_EQ(nodeId(Node_fn), m_node.id);
    EXPECT_AST_FIELD_AR(Node_fn_params(&m_node), fields);
    EXPECT_AST(Node_fn_ret_type(&m_node), &n_t_hello);
    EXPECT_AST(Node_fn_body(&m_node), &n_t_hello);

    test(m_ast.make_fn(fields, n_t_hello, n_t_hello, true));
    EXPECT_EQ(nodeId(Node_fn_var), m_node.id);
    EXPECT_AST_FIELD_AR(Node_fn_params(&m_node), fields);
    EXPECT_AST(Node_fn_ret_type(&m_node), &n_t_hello);
    EXPECT_AST(Node_fn_body(&m_node), &n_t_hello);

    test(m_ast.make_tag(&t_hello, args, n_t_hello));
    EXPECT_EQ(nodeId(Node_tag), m_node.id);
    EXPECT_EQ(Node_tag_tag(&m_node), &t_hello);
    EXPECT_AST_NN_AR(Node_tag_args(&m_node), args);
    EXPECT_AST(Node_tag_node(&m_node), &n_t_hello);

    test(m_ast.make_call(n_t_hello, args));
    EXPECT_EQ(nodeId(Node_call), m_node.id);
    EXPECT_AST(Node_call_lhs(&m_node), &n_t_hello);
    EXPECT_AST_NN_AR(Node_call_args(&m_node), args);

    test(m_ast.make_object_literal(n_t_hello, args));
    EXPECT_EQ(nodeId(Node_object_literal), m_node.id);
    EXPECT_AST(Node_call_lhs(&m_node), &n_t_hello);
    EXPECT_AST_NN_AR(Node_call_args(&m_node), args);

    test(m_ast.make_assign(nodes, n_t_hello));
    EXPECT_EQ(nodeId(Node_assign), m_node.id);
    EXPECT_AST_AR(Node_assign_lhs(&m_node), nodes);
    EXPECT_AST(Node_assign_value(&m_node), &n_t_hello);

    test(m_ast.make_define(tokens, n_t_hello));
    EXPECT_EQ(nodeId(Node_define), m_node.id);
    EXPECT_AST_TOKEN_AR(Node_define_names(&m_node), tokens);
    EXPECT_AST(Node_define_value(&m_node), &n_t_hello);

    test(m_ast.make_comptime_const_def(&t_hello, n_t_hello));
    EXPECT_EQ(nodeId(Node_comptime_const_def), m_node.id);
    EXPECT_EQ(Node_comptime_const_def_name(&m_node), &t_hello);
    EXPECT_AST(Node_comptime_const_def_value(&m_node), &n_t_hello);

    test(m_ast.make_tag_def(&t_hello, n_t_hello));
    EXPECT_EQ(nodeId(Node_tag_def), m_node.id);
    EXPECT_EQ(Node_comptime_const_def_name(&m_node), &t_hello);
    EXPECT_AST(Node_comptime_const_def_value(&m_node), &n_t_hello);

    test(m_ast.make_var_decl(&t_hello, n_t_hello, n_t_hello));
    EXPECT_EQ(nodeId(Node_var_decl), m_node.id);
    EXPECT_EQ(Node_var_decl_name(&m_node), &t_hello);
    EXPECT_AST(Node_var_decl_type(&m_node), &n_t_hello);
    EXPECT_AST(Node_var_decl_value(&m_node), &n_t_hello);

    test(m_ast.make_const_decl(&t_hello, n_t_hello, n_t_hello));
    EXPECT_EQ(nodeId(Node_const_decl), m_node.id);
    EXPECT_EQ(Node_var_decl_name(&m_node), &t_hello);
    EXPECT_AST(Node_var_decl_type(&m_node), &n_t_hello);
    EXPECT_AST(Node_var_decl_value(&m_node), &n_t_hello);
}
