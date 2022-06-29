#include "nkl/lang/ast.hpp"

namespace nkl {

char const *s_ast_node_names[] = {
#define X(TYPE, ID) #ID,
#include "nkl/lang/nodes.inl"
};

namespace {

Id _nodeId(ENodeId node_id) {
    return cs2id(s_ast_node_names[node_id]);
}

} // namespace

#define N(TYPE, ID)                          \
    Node LangAst::make_##ID() {              \
        return Node{{}, _nodeId(Node_##ID)}; \
    }
#define U(TYPE, ID)                                           \
    Node LangAst::make_##ID(Node const &arg) {                \
        return Node{{push(arg), {}, {}}, _nodeId(Node_##ID)}; \
    }
#define B(TYPE, ID)                                                  \
    Node LangAst::make_##ID(Node const &lhs, Node const &rhs) {      \
        return Node{{push(lhs), push(rhs), {}}, _nodeId(Node_##ID)}; \
    }
#include "nkl/lang/nodes.inl"

Node LangAst::make_if(Node const &cond, Node const &then_clause, Node const &else_clause) {
    return Node{{push(cond), push(then_clause), push(else_clause)}, _nodeId(Node_if)};
}

Node LangAst::make_ternary(Node const &cond, Node const &then_clause, Node const &else_clause) {
    return Node{{push(cond), push(then_clause), push(else_clause)}, _nodeId(Node_ternary)};
}

Node LangAst::make_array(NodeArray nodes) {
    return Node{{push(nodes), {}, {}}, _nodeId(Node_array)};
}

Node LangAst::make_block(NodeArray nodes) {
    return Node{{push(nodes), {}, {}}, _nodeId(Node_block)};
}

Node LangAst::make_tuple(NodeArray nodes) {
    return Node{{push(nodes), {}, {}}, _nodeId(Node_tuple)};
}

Node LangAst::make_id_tuple(NodeArray nodes) {
    return Node{{push(nodes), {}, {}}, _nodeId(Node_id_tuple)};
}

Node LangAst::make_tuple_type(NodeArray nodes) {
    return Node{{push(nodes), {}, {}}, _nodeId(Node_tuple_type)};
}

Node LangAst::make_id(TokenRef name) {
    return Node{{push(name), {}, {}}, _nodeId(Node_id)};
}

Node LangAst::make_numeric_float(TokenRef val) {
    return Node{{push(val), {}, {}}, _nodeId(Node_numeric_float)};
}

Node LangAst::make_numeric_int(TokenRef val) {
    return Node{{push(val), {}, {}}, _nodeId(Node_numeric_int)};
}

Node LangAst::make_string_literal(TokenRef val) {
    return Node{{push(val), {}, {}}, _nodeId(Node_string_literal)};
}

Node LangAst::make_escaped_string_literal(TokenRef val) {
    return Node{{push(val), {}, {}}, _nodeId(Node_escaped_string_literal)};
}

Node LangAst::make_member(Node const &lhs, TokenRef name) {
    return Node{{push(lhs), push(name), {}}, _nodeId(Node_member)};
}

Node LangAst::make_struct(TokenRef name, NamedNodeArray fields) {
    return Node{{push(name), push(fields), {}}, _nodeId(Node_member)};
}

Node LangAst::make_call(Node const &lhs, NodeArray args) {
    return Node{{push(lhs), push(args), {}}, _nodeId(Node_call)};
}

Node LangAst::make_fn(
    TokenRef name,
    NamedNodeArray params,
    Node const &ret_type,
    Node const &body) {
    return Node{
        {push(Node{{push(name), push(params), push(ret_type)}, 0}), push(body), {}},
        _nodeId(Node_fn)};
}

Node LangAst::make_struct_literal(Node const &type, NamedNodeArray fields) {
    return Node{{push(type), push(fields), {}}, _nodeId(Node_struct_literal)};
}

Node LangAst::make_var_decl(TokenRef name, Node const &type, Node const &value) {
    return Node{{push(name), push(type), push(value)}, _nodeId(Node_struct_literal)};
}

} // namespace nkl
