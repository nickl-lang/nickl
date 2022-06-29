#ifndef HEADER_GUARD_NKL_LANG_AST
#define HEADER_GUARD_NKL_LANG_AST

#include "nkl/core/ast.hpp"

namespace nkl {

enum ENodeId {
#define X(TYPE, ID) Node_##ID,
#include "nkl/lang/nodes.inl"

    Node_count,
};

extern char const *s_ast_node_names[];

struct LangAst : Ast {
#define N(TYPE, ID) Node make_##ID();
#define U(TYPE, ID) Node make_##ID(Node const &arg);
#define B(TYPE, ID) Node make_##ID(Node const &lhs, Node const &rhs);
#include "nkl/lang/nodes.inl"

    Node make_if(Node const &cond, Node const &then_clause, Node const &else_clause);
    Node make_ternary(Node const &cond, Node const &then_clause, Node const &else_clause);

    Node make_array(NodeArray nodes);
    Node make_block(NodeArray nodes);
    Node make_tuple(NodeArray nodes);
    Node make_id_tuple(NodeArray nodes);
    Node make_tuple_type(NodeArray nodes);

    Node make_id(TokenRef name);
    Node make_numeric_float(TokenRef val);
    Node make_numeric_int(TokenRef val);
    Node make_string_literal(TokenRef val);
    Node make_escaped_string_literal(TokenRef val);

    Node make_member(Node const &lhs, TokenRef name);

    Node make_struct(TokenRef name, NamedNodeArray fields);

    Node make_call(Node const &lhs, NodeArray args);
    Node make_fn(TokenRef name, NamedNodeArray params, Node const &ret_type, Node const &body);
    Node make_struct_literal(Node const &type, NamedNodeArray fields);
    Node make_var_decl(TokenRef name, Node const &type, Node const &value);

private:
    using Ast::push;
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_AST
