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

void LangAst::init() {
    Ast::init();
}

void LangAst::deinit() {
    m_arena.deinit();

    Ast::deinit();
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

Node LangAst::make_tuple_type(NodeArray nodes) {
    return Node{{push(nodes), {}, {}}, nodeId(Node_tuple_type)};
}

Node LangAst::make_run(NodeArray nodes) {
    return Node{{push(nodes), {}, {}}, nodeId(Node_run)};
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

Node LangAst::make_import_path(TokenRef path) {
    return Node{{push(path), {}, {}}, nodeId(Node_import_path)};
}

Node LangAst::make_import(TokenRefArray names) {
    return Node{{push(names), {}, {}}, nodeId(Node_import)};
}

Node LangAst::make_for(TokenRef it, Node const &range, Node const &body) {
    return Node{{push(it), push(range), push(body)}, nodeId(Node_for)};
}

Node LangAst::make_for_by_ptr(TokenRef it, Node const &range, Node const &body) {
    return Node{{push(it), push(range), push(body)}, nodeId(Node_for_by_ptr)};
}

Node LangAst::make_member(Node const &lhs, TokenRef name) {
    return Node{{push(lhs), push(name), {}}, nodeId(Node_member)};
}

Node LangAst::make_struct(FieldNodeArray fields) {
    return Node{{push(fields), {}, {}}, nodeId(Node_struct)};
}

Node LangAst::make_union(FieldNodeArray fields) {
    return Node{{push(fields), {}, {}}, nodeId(Node_union)};
}

Node LangAst::make_enum(FieldNodeArray fields) {
    return Node{{push(fields), {}, {}}, nodeId(Node_enum)};
}

Node LangAst::make_packed_struct(FieldNodeArray fields) {
    return Node{{push(fields), {}, {}}, nodeId(Node_packed_struct)};
}

Node LangAst::make_fn(
    FieldNodeArray params,
    Node const &ret_type,
    Node const &body,
    bool is_variadic) {
    return Node{
        {push(params), push(ret_type), push(body)}, nodeId(is_variadic ? Node_fn_var : Node_fn)};
}

Node LangAst::make_tag(TokenRef tag, NamedNodeArray args, Node const &node) {
    return Node{{push(tag), push(args), push(node)}, nodeId(Node_tag)};
}

Node LangAst::make_call(Node const &lhs, NamedNodeArray args) {
    return Node{{push(lhs), push(args), {}}, nodeId(Node_call)};
}

Node LangAst::make_object_literal(Node const &lhs, NamedNodeArray args) {
    return Node{{push(lhs), push(args), {}}, nodeId(Node_object_literal)};
}

Node LangAst::make_assign(NodeArray lhs, Node const &value) {
    return Node{{push(lhs), push(value), {}}, nodeId(Node_assign)};
}

Node LangAst::make_define(TokenRefArray names, Node const &value) {
    return Node{{push(names), push(value), {}}, nodeId(Node_define)};
}

Node LangAst::make_comptime_const_def(TokenRef name, Node const &value) {
    return Node{{push(name), push(value), {}}, nodeId(Node_comptime_const_def)};
}

Node LangAst::make_tag_def(TokenRef name, Node const &type) {
    return Node{{push(name), push(type), {}}, nodeId(Node_tag_def)};
}

Node LangAst::make_var_decl(TokenRef name, Node const &type, Node const &value) {
    return Node{{push(name), push(type), push(value)}, nodeId(Node_var_decl)};
}

Node LangAst::make_const_decl(TokenRef name, Node const &type, Node const &value) {
    return Node{{push(name), push(type), push(value)}, nodeId(Node_const_decl)};
}

NodeArg LangAst::push(NamedNodeArray nns) {
    auto const frame = m_arena.pushFrame();
    defer {
        m_arena.popFrame(frame);
    };

    auto args = m_arena.alloc<NodeArg>(nns.size);
    for (size_t i = 0; auto const &nn : nns) {
        args[i++] = {nn.name, {nn.node, 1}};
    }

    return push(args);
}

NodeArg LangAst::push(FieldNodeArray fields) {
    auto const frame = m_arena.pushFrame();
    defer {
        m_arena.popFrame(frame);
    };

    auto nodes = m_arena.alloc<Node>(fields.size);
    for (size_t i = 0; auto const &field : fields) {
        nodes[i++] = {
            {push(field.name), push(field.type), push(field.init_value)},
            field.is_const ? cs2id("const") : cs2id("mut")};
    }

    return push((NodeArray)nodes);
}

NodeArg LangAst::push(TokenRefArray tokens) {
    auto const frame = m_arena.pushFrame();
    defer {
        m_arena.popFrame(frame);
    };

    auto args = m_arena.alloc<NodeArg>(tokens.size);
    for (size_t i = 0; auto const &token : tokens) {
        args[i++] = {token, {}};
    }

    return push(args);
}

NamedNode PackedNamedNodeArray::operator[](size_t i) const {
    auto const &arg = PackedNodeArgArray::operator[](i);
    return {
        .name = arg.token,
        .node = arg.nodes.begin(),
    };
}

PackedFieldNodeArray::PackedFieldNodeArray(NodeArray fields)
    : NodeArray{fields} {
}

FieldNode PackedFieldNodeArray::operator[](size_t i) const {
    auto const &node = NodeArray::operator[](i);
    return {
        .name = node.arg[0].token,
        .type = node.arg[1].nodes.begin(),
        .init_value = node.arg[2].nodes.begin(),
        .is_const = node.id == cs2id("const"),
    };
}

TokenRef PackedTokenRefArray::operator[](size_t i) const {
    auto const &arg = PackedNodeArgArray::operator[](i);
    return arg.token;
}

} // namespace nkl
