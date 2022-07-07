#ifndef HEADER_GUARD_NKL_LANG_AST
#define HEADER_GUARD_NKL_LANG_AST

#include "nk/utils/utils.hpp"
#include "nkl/core/ast.hpp"

namespace nkl {

enum ENodeId {
#define X(ID) CAT(Node_, ID),
#include "nkl/lang/nodes.inl"

    Node_count,
};

Id nodeId(ENodeId id);

struct LangAst : Ast {
#define N(ID) Node CAT(make_, ID)();
#define U(ID) Node CAT(make_, ID)(Node const &arg);
#define B(ID) Node CAT(make_, ID)(Node const &lhs, Node const &rhs);
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

#define _NodeArg(NODE, IDX) (NODE)->arg[IDX]

#define _NodeArgAsToken(NODE, IDX) (_NodeArg(NODE, IDX).token)
#define _NodeArgAsNode(NODE, IDX) (_NodeArg(NODE, IDX).nodes.begin())
#define _NodeArgAsAr(NODE, IDX) (_NodeArg(NODE, IDX).nodes)
#define _NodeArgAsNnAr(NODE, IDX) (PackedNamedNodeArray{_NodeArg(NODE, IDX).nodes})

#define Node_unary_arg(NODE) _NodeArgAsNode((NODE), 0)

#define Node_binary_lhs(NODE) _NodeArgAsNode((NODE), 0)
#define Node_binary_rhs(NODE) _NodeArgAsNode((NODE), 1)

#define Node_ternary_cond(NODE) _NodeArgAsNode((NODE), 0)
#define Node_ternary_then_clause(NODE) _NodeArgAsNode((NODE), 1)
#define Node_ternary_else_clause(NODE) _NodeArgAsNode((NODE), 2)

#define Node_array_nodes(NODE) _NodeArgAsAr((NODE), 0)

#define Node_token_name(NODE) _NodeArgAsToken((NODE), 0)

#define Node_member_lhs(NODE) _NodeArgAsNode((NODE), 0)
#define Node_member_name(NODE) _NodeArgAsToken((NODE), 1)

#define Node_struct_name(NODE) _NodeArgAsToken((NODE), 0)
#define Node_struct_fields(NODE) _NodeArgAsNnAr((NODE), 1)

#define Node_call_lhs(NODE) _NodeArgAsNode((NODE), 0)
#define Node_call_args(NODE) _NodeArgAsAr((NODE), 1)

#define Node_fn_name(NODE) _NodeArgAsToken(_NodeArgAsNode((NODE), 0), 0)
#define Node_fn_params(NODE) _NodeArgAsNnAr(_NodeArgAsNode((NODE), 0), 1)
#define Node_fn_ret_type(NODE) _NodeArgAsNode(_NodeArgAsNode((NODE), 0), 2)
#define Node_fn_body(NODE) _NodeArgAsNode((NODE), 1)

#define Node_struct_literal_type(NODE) _NodeArgAsNode((NODE), 0)
#define Node_struct_literal_fields(NODE) _NodeArgAsNnAr((NODE), 1)

#define Node_var_decl_name(NODE) _NodeArgAsToken((NODE), 0)
#define Node_var_decl_type(NODE) _NodeArgAsNode((NODE), 1)
#define Node_var_decl_value(NODE) _NodeArgAsNode((NODE), 2)

#endif // HEADER_GUARD_NKL_LANG_AST
