#include "nkl/lang/ast.hpp"

namespace nkl {

namespace {

char const *s_ast_node_names[] = {
#define X(ID) #ID,
#include "nkl/lang/nodes.inl"
};

} // namespace

Id nodeId(ENodeId node_id) {
    return cs2id(s_ast_node_names[node_id]);
}

#define N(ID)                                    \
    Node LangAst::CAT(make_, ID)() {             \
        return Node{{}, nodeId(CAT(Node_, ID))}; \
    }
#define U(ID)                                                     \
    Node LangAst::CAT(make_, ID)(Node const &arg) {               \
        return Node{{push(arg), {}, {}}, nodeId(CAT(Node_, ID))}; \
    }
#define B(ID)                                                            \
    Node LangAst::CAT(make_, ID)(Node const &lhs, Node const &rhs) {     \
        return Node{{push(lhs), push(rhs), {}}, nodeId(CAT(Node_, ID))}; \
    }
#include "nkl/lang/nodes.inl"

Node LangAst::make_if(Node const &cond, Node const &then_clause, Node const &else_clause) {
    return Node{{push(cond), push(then_clause), push(else_clause)}, nodeId(Node_if)};
}

Node LangAst::make_ternary(Node const &cond, Node const &then_clause, Node const &else_clause) {
    return Node{{push(cond), push(then_clause), push(else_clause)}, nodeId(Node_ternary)};
}

Node LangAst::make_array(NodeArray nodes) {
    return Node{{push(nodes), {}, {}}, nodeId(Node_array)};
}

Node LangAst::make_block(NodeArray nodes) {
    return Node{{push(nodes), {}, {}}, nodeId(Node_block)};
}

Node LangAst::make_tuple(NodeArray nodes) {
    return Node{{push(nodes), {}, {}}, nodeId(Node_tuple)};
}

Node LangAst::make_id_tuple(NodeArray nodes) {
    return Node{{push(nodes), {}, {}}, nodeId(Node_id_tuple)};
}

Node LangAst::make_tuple_type(NodeArray nodes) {
    return Node{{push(nodes), {}, {}}, nodeId(Node_tuple_type)};
}

Node LangAst::make_id(TokenRef name) {
    return Node{{push(name), {}, {}}, nodeId(Node_id)};
}

Node LangAst::make_numeric_float(TokenRef val) {
    return Node{{push(val), {}, {}}, nodeId(Node_numeric_float)};
}

Node LangAst::make_numeric_int(TokenRef val) {
    return Node{{push(val), {}, {}}, nodeId(Node_numeric_int)};
}

Node LangAst::make_string_literal(TokenRef val) {
    return Node{{push(val), {}, {}}, nodeId(Node_string_literal)};
}

Node LangAst::make_escaped_string_literal(TokenRef val) {
    return Node{{push(val), {}, {}}, nodeId(Node_escaped_string_literal)};
}

Node LangAst::make_member(Node const &lhs, TokenRef name) {
    return Node{{push(lhs), push(name), {}}, nodeId(Node_member)};
}

Node LangAst::make_struct(TokenRef name, NamedNodeArray fields) {
    return Node{{push(name), push(fields), {}}, nodeId(Node_struct)};
}

Node LangAst::make_call(Node const &lhs, NodeArray args) {
    return Node{{push(lhs), push(args), {}}, nodeId(Node_call)};
}

Node LangAst::make_fn(
    TokenRef name,
    NamedNodeArray params,
    Node const &ret_type,
    Node const &body) {
    return Node{
        {push(Node{{push(name), push(params), push(ret_type)}, 0}), push(body), {}},
        nodeId(Node_fn)};
}

Node LangAst::make_struct_literal(Node const &type, NamedNodeArray fields) {
    return Node{{push(type), push(fields), {}}, nodeId(Node_struct_literal)};
}

Node LangAst::make_var_decl(TokenRef name, Node const &type, Node const &value) {
    return Node{{push(name), push(type), push(value)}, nodeId(Node_var_decl)};
}

} // namespace nkl
