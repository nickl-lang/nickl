#ifndef HEADER_GUARD_NKL_LANG_AST
#define HEADER_GUARD_NKL_LANG_AST

#include "nk/common/utils.hpp"
#include "nkl/core/ast.hpp"

namespace nkl {

enum ENodeId {
#define X(TYPE, ID) CAT(Node_, ID),
#include "nkl/lang/nodes.inl"

    Node_count,
};

Id nodeId(ENodeId id);

struct LangAst : Ast {
#define N(TYPE, ID) Node CAT(make_, ID)();
#define U(TYPE, ID) Node CAT(make_, ID)(Node const &arg);
#define B(TYPE, ID) Node CAT(make_, ID)(Node const &lhs, Node const &rhs);
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

#define _NodeArg0(NODE) (NODE)->arg[0]
#define _NodeArg1(NODE) (NODE)->arg[1]
#define _NodeArg2(NODE) (NODE)->arg[2]

#define _NodeArgAsToken(ARG) (ARG.token)
#define _NodeArgAsNode(ARG) (ARG.nodes.begin())
#define _NodeArgAsAr(ARG) (ARG.nodes)
#define _NodeArgAsNnAr(ARG) (PackedNamedNodeArray{ARG.nodes})

#define Node_unary_arg(NODE) _NodeArgAsNode(_NodeArg0(NODE))

#define Node_binary_lhs(NODE) _NodeArgAsNode(_NodeArg0(NODE))
#define Node_binary_rhs(NODE) _NodeArgAsNode(_NodeArg1(NODE))

#define Node_ternary_cond(NODE) _NodeArgAsNode(_NodeArg0(NODE))
#define Node_ternary_then_clause(NODE) _NodeArgAsNode(_NodeArg1(NODE))
#define Node_ternary_else_clause(NODE) _NodeArgAsNode(_NodeArg2(NODE))

#define Node_array_nodes(NODE) _NodeArgAsAr(_NodeArg0(NODE))

#define Node_token_name(NODE) _NodeArgAsToken(_NodeArg0(NODE))

#define Node_member_lhs(NODE) _NodeArgAsNode(_NodeArg0(NODE))
#define Node_member_name(NODE) _NodeArgAsToken(_NodeArg1(NODE))

#define Node_struct_name(NODE) _NodeArgAsToken(_NodeArg0(NODE))
#define Node_struct_fields(NODE) _NodeArgAsNnAr(_NodeArg1(NODE))

#define Node_call_lhs(NODE) _NodeArgAsNode(_NodeArg0(NODE))
#define Node_call_args(NODE) _NodeArgAsAr(_NodeArg1(NODE))

#define Node_fn_name(NODE) _NodeArgAsToken(_NodeArg0(_NodeArgAsNode(_NodeArg0(NODE))))
#define Node_fn_params(NODE) _NodeArgAsNnAr(_NodeArg1(_NodeArgAsNode(_NodeArg0(NODE))))
#define Node_fn_ret_type(NODE) _NodeArgAsNode(_NodeArg2(_NodeArgAsNode(_NodeArg0(NODE))))
#define Node_fn_body(NODE) _NodeArgAsNode(_NodeArg1(NODE))

#define Node_struct_literal_type(NODE) _NodeArgAsNode(_NodeArg0(NODE))
#define Node_struct_literal_fields(NODE) _NodeArgAsNnAr(_NodeArg1(NODE))

#define Node_var_decl_name(NODE) _NodeArgAsToken(_NodeArg0(NODE))
#define Node_var_decl_type(NODE) _NodeArgAsNode(_NodeArg1(NODE))
#define Node_var_decl_value(NODE) _NodeArgAsNode(_NodeArg2(NODE))

#endif // HEADER_GUARD_NKL_LANG_AST
