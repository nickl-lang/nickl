#include "nkl/core/ast.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iterator>
#include <sstream>
#include <string>

namespace nkl {

#define AST_INIT_CAPACITY 1024

char const *s_ast_node_names[] = {
#define X(TYPE, ID) #ID,
#include "nkl/core//nodes.inl"
};

void Ast::init() {
    data.init(AST_INIT_CAPACITY);
}

void Ast::deinit() {
    data.deinit();
}

#define N(TYPE, ID)                            \
    Node Ast::make_##ID() {                    \
        return Node{{.TYPE = {0}}, Node_##ID}; \
    }
#define U(TYPE, ID)                                    \
    Node Ast::make_##ID(Node const &arg) {             \
        return Node{{.TYPE = {push(arg)}}, Node_##ID}; \
    }
#define B(TYPE, ID)                                               \
    Node Ast::make_##ID(Node const &lhs, Node const &rhs) {       \
        return Node{{.TYPE = {push(lhs), push(rhs)}}, Node_##ID}; \
    }
#include "nkl/core/nodes.inl"

Node Ast::make_if(Node const &cond, Node const &then_clause, Node const &else_clause) {
    return Node{{.ternary = {push(cond), push(then_clause), push(else_clause)}}, Node_if};
}

Node Ast::make_ternary(Node const &cond, Node const &then_clause, Node const &else_clause) {
    return Node{{.ternary = {push(cond), push(then_clause), push(else_clause)}}, Node_ternary};
}

Node Ast::make_array(NodeArray nodes) {
    return Node{{.array = {push_ar(nodes)}}, Node_array};
}

Node Ast::make_block(NodeArray nodes) {
    return Node{{.array = {push_ar(nodes)}}, Node_block};
}

Node Ast::make_tuple(NodeArray nodes) {
    return Node{{.array = {push_ar(nodes)}}, Node_tuple};
}

Node Ast::make_id_tuple(NodeArray nodes) {
    return Node{{.array = {push_ar(nodes)}}, Node_id_tuple};
}

Node Ast::make_tuple_type(NodeArray nodes) {
    return Node{{.array = {push_ar(nodes)}}, Node_tuple_type};
}

Node Ast::make_id(token_ref_t name) {
    return Node{{.token = {name}}, Node_id};
}

Node Ast::make_numeric_int(token_ref_t val) {
    return Node{{.token = {val}}, Node_numeric_int};
}

Node Ast::make_numeric_float(token_ref_t val) {
    return Node{{.token = {val}}, Node_numeric_float};
}

Node Ast::make_string_literal(token_ref_t val) {
    return Node{{.token = {val}}, Node_string_literal};
}

Node Ast::make_member(Node const &lhs, token_ref_t name) {
    return Node{{.member = {push(lhs), name}}, Node_member};
}

Node Ast::make_struct(token_ref_t name, NamedNodeArray fields) {
    return Node{{.type_decl = {name, push_named_ar(fields)}}, Node_struct};
}

Node Ast::make_call(Node const &lhs, NodeArray args) {
    return Node{{.call = {push(lhs), push_ar(args)}}, Node_call};
}

Node Ast::make_fn(token_ref_t name, NamedNodeArray params, Node const &ret_type, Node const &body) {
    return Node{
        {.fn = {{name, push_named_ar(params), push(ret_type)}, push(body), nullptr, false}},
        Node_fn};
}

Node Ast::make_foreign_fn(
    token_ref_t lib,
    token_ref_t name,
    NamedNodeArray params,
    Node const &ret_type,
    bool is_variadic) {
    return Node{
        {.fn = {{name, push_named_ar(params), push(ret_type)}, n_none_ref, lib, is_variadic}},
        Node_foreign_fn};
}

Node Ast::make_struct_literal(Node const &type, NamedNodeArray fields) {
    return Node{{.struct_literal = {push(type), push_named_ar(fields)}}, Node_struct_literal};
}

Node Ast::make_var_decl(token_ref_t name, Node const &type, Node const &value) {
    return Node{{.var_decl = {name, push(type), push(value)}}, Node_var_decl};
}

node_ref_t Ast::push(Node node) {
    Node *ref = &data.push();
    *ref = node;
    return ref;
}

NodeArray Ast::push_ar(NodeArray nodes) {
    Node *first = &data.push(nodes.size);
    //@Refacor Wrap explicit memcpy in AST node array allocation
    std::memcpy(first, nodes.data, nodes.size * sizeof(Node));
    return NodeArray{first, nodes.size};
}

NodeArray Ast::push_named_ar(NamedNodeArray nodes) {
    Node *first = &data.push(nodes.size);
    NodeArray const ar{first, nodes.size};
    for (auto pn = nodes.data; pn < nodes.data + nodes.size; pn++, first++) {
        first->as.named_node = *pn;
    }
    return ar;
}

namespace {

void _inspect(node_ref_t node, std::ostringstream &ss, size_t depth = 1) {
    assert(depth > 0);

    auto const newline = [&](size_t depth) {
        static constexpr size_t c_indent_size = 2;
        ss << '\n';
        std::fill_n(std::ostream_iterator<char>(ss), depth * c_indent_size, ' ');
    };

    auto const field = [&](const char *name) {
        ss << ',';
        newline(depth);
        ss << name << ": ";
    };

    auto const inspectNodeArray = [&](NodeArray const &ar, size_t depth) {
        ss << "[";
        bool first = true;
        for (size_t i = 0; i < ar.size; i++) {
            if (!first) {
                ss << ", ";
            }
            _inspect(ar.data + i, ss, depth + 1);
            first = false;
        }
        ss << "]";
    };

    auto const inspectNamedNodeArray = [&](NodeArray const &ar, size_t depth) {
        ss << "[";
        bool first = true;
        for (node_ref_t node = ar.data; node != ar.data + ar.size; node++) {
            if (!first) {
                ss << ", ";
            }
            ss << "{";
            newline(depth + 1);
            ss << "name: #" << node->as.named_node.name->text << ",";
            newline(depth + 1);
            ss << "node: ";
            _inspect(node->as.named_node.node, ss, depth + 2);
            ss << "}";
            first = false;
        }
        ss << "]";
    };

    auto const inspectSig = [&](_fn_type const &sig, size_t depth) {
        ss << "{";
        newline(depth + 1);
        ss << "params: ";
        inspectNamedNodeArray(sig.params, depth + 1);
        ss << ",";
        newline(depth + 1);
        ss << "ret_type: ";
        _inspect(sig.ret_type, ss, depth + 2);
        ss << "}";
    };

    auto const inspectNode = [&](node_ref_t node) {
        _inspect(node, ss, depth + 1);
    };

    auto const inspectToken = [&](token_ref_t token) {
        if (token) {
            ss << "(" << s_token_id[token->id] << ", \"" << token->text << "\")";
        } else {
            ss << "null";
        }
    };

    auto const inspectBool = [&](bool v) {
        ss << std::boolalpha << v;
    };

    if (!node) {
        ss << "null";
        return;
    }

    ss << "{";
    newline(depth);
    ss << "id: " << s_ast_node_names[node->id];

    switch (node->id) {
    default:
        break;

#define U(TYPE, ID) case Node_##ID:
#include "nkl/core/nodes.inl"
        field("arg");
        inspectNode(node->as.unary.arg);
        break;

#define B(TYPE, ID) case Node_##ID:
#include "nkl/core/nodes.inl"
        field("lhs");
        inspectNode(node->as.binary.lhs);
        field("rhs");
        inspectNode(node->as.binary.rhs);
        break;

    case Node_if:
    case Node_ternary:
        field("arg1");
        inspectNode(node->as.ternary.arg1);
        field("arg2");
        inspectNode(node->as.ternary.arg2);
        field("arg3");
        inspectNode(node->as.ternary.arg3);
        break;

    case Node_array:
    case Node_block:
    case Node_tuple:
    case Node_id_tuple:
    case Node_tuple_type:
        field("nodes");
        inspectNodeArray(node->as.array.nodes, depth);
        break;

    case Node_struct:
        field("name");
        inspectToken(node->as.type_decl.name);
        field("fields");
        inspectNamedNodeArray(node->as.type_decl.fields, depth);
        break;

    case Node_member:
        field("lhs");
        inspectNode(node->as.member.lhs);
        field("name");
        inspectToken(node->as.member.name);
        break;

    case Node_id:
    case Node_numeric_float:
    case Node_numeric_int:
    case Node_string_literal:
        field("val");
        inspectToken(node->as.token.val);
        break;

    case Node_call:
        field("lhs");
        inspectNode(node->as.call.lhs);
        field("args");
        inspectNodeArray(node->as.call.args, depth);
        break;

    case Node_var_decl:
        field("name");
        inspectToken(node->as.var_decl.name);
        field("type");
        inspectNode(node->as.var_decl.type);
        field("value");
        inspectNode(node->as.var_decl.value);
        break;

    case Node_fn:
        field("name");
        inspectToken(node->as.fn.sig.name);
        field("sig");
        inspectSig(node->as.fn.sig, depth);
        field("body");
        inspectNode(node->as.fn.body);
        break;

    case Node_foreign_fn:
        field("lib");
        inspectToken(node->as.fn.lib);
        field("name");
        inspectToken(node->as.fn.sig.name);
        field("sig");
        inspectSig(node->as.fn.sig, depth);
        field("is_variadic");
        inspectBool(node->as.fn.is_variadic);
        break;

    case Node_struct_literal:
        field("type");
        inspectNode(node->as.struct_literal.type);
        field("fields");
        inspectNamedNodeArray(node->as.struct_literal.fields, depth);
        break;
    }

    ss << "}";
}

} // namespace

string ast_inspect(node_ref_t node) {
    std::ostringstream ss;
    _inspect(node, ss);
    auto str = ss.str();

    string res;
    string{str.data(), str.size()}.copy(res, *_mctx.tmp_allocator);
    return res;
}

} // namespace nkl
