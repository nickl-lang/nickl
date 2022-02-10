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
    strings.init();
}

void Ast::deinit() {
    strings.deinit();
    data.deinit();
}

#define N(TYPE, ID)                            \
    Node Ast::make_##ID() {                    \
        return Node{{.TYPE = {0}}, Node_##ID}; \
    }
#define U(TYPE, ID)                              \
    Node Ast::make_##ID(node_ref_t arg) {        \
        return Node{{.TYPE = {arg}}, Node_##ID}; \
    }
#define B(TYPE, ID)                                       \
    Node Ast::make_##ID(node_ref_t lhs, node_ref_t rhs) { \
        return Node{{.TYPE = {lhs, rhs}}, Node_##ID};     \
    }
#include "nkl/core/nodes.inl"

Node Ast::make_if(node_ref_t cond, node_ref_t then_clause, node_ref_t else_clause) {
    return Node{{.ternary = {cond, then_clause, else_clause}}, Node_if};
}

Node Ast::make_ternary(node_ref_t cond, node_ref_t then_clause, node_ref_t else_clause) {
    return Node{{.ternary = {cond, then_clause, else_clause}}, Node_ternary};
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

Node Ast::make_tuple_type(NodeArray nodes) {
    return Node{{.array = {push_ar(nodes)}}, Node_tuple_type};
}

Node Ast::make_id(Id name) {
    return Node{{.id = {name}}, Node_id};
}

Node Ast::make_member(node_ref_t lhs, Id name) {
    return Node{{.member = {lhs, name}}, Node_member};
}

Node Ast::make_numeric_i8(int8_t val) {
    return Node{{.numeric = {.val = {.i8 = val}}}, Node_numeric_i8};
}

Node Ast::make_numeric_i16(int16_t val) {
    return Node{{.numeric = {.val = {.i16 = val}}}, Node_numeric_i16};
}

Node Ast::make_numeric_i32(int32_t val) {
    return Node{{.numeric = {.val = {.i32 = val}}}, Node_numeric_i32};
}

Node Ast::make_numeric_i64(int64_t val) {
    return Node{{.numeric = {.val = {.i64 = val}}}, Node_numeric_i64};
}

Node Ast::make_numeric_u8(uint8_t val) {
    return Node{{.numeric = {.val = {.u8 = val}}}, Node_numeric_u8};
}

Node Ast::make_numeric_u16(uint16_t val) {
    return Node{{.numeric = {.val = {.u16 = val}}}, Node_numeric_u16};
}

Node Ast::make_numeric_u32(uint32_t val) {
    return Node{{.numeric = {.val = {.u32 = val}}}, Node_numeric_u32};
}

Node Ast::make_numeric_u64(uint64_t val) {
    return Node{{.numeric = {.val = {.u64 = val}}}, Node_numeric_u64};
}

Node Ast::make_numeric_f32(float val) {
    return Node{{.numeric = {.val = {.f32 = val}}}, Node_numeric_f32};
}

Node Ast::make_numeric_f64(double val) {
    return Node{{.numeric = {.val = {.f64 = val}}}, Node_numeric_f64};
}

Node Ast::make_struct(Id name, NamedNodeArray fields) {
    return Node{{.type_decl = {name, push_named_ar(fields)}}, Node_struct};
}

Node Ast::make_call(node_ref_t lhs, NodeArray args) {
    return Node{{.call = {lhs, push_ar(args)}}, Node_call};
}

Node Ast::make_fn(Id name, NamedNodeArray params, node_ref_t ret_type, node_ref_t body) {
    return Node{{.fn = {{name, push_named_ar(params), ret_type}, body}}, Node_fn};
}

Node Ast::make_string_literal(string str) {
    string ast_str;
    str.copy(ast_str, strings);
    return Node{{.str = {ast_str}}, Node_string_literal};
}

Node Ast::make_struct_literal(node_ref_t type, NamedNodeArray fields) {
    return Node{{.struct_literal = {type, push_named_ar(fields)}}, Node_struct_literal};
}

Node Ast::make_var_decl(Id name, node_ref_t type, node_ref_t value) {
    return Node{{.var_decl = {name, type, value}}, Node_var_decl};
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
            ss << "name: #" << id2s(node->as.named_node.name) << ",";
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

    auto const inspectId = [&](Id id) {
        if (id) {
            ss << "#" << id2s(id);
        } else {
            ss << "null";
        }
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
    case Node_tuple_type:
        field("nodes");
        inspectNodeArray(node->as.array.nodes, depth);
        break;

    case Node_struct:
        field("name");
        inspectId(node->as.type_decl.name);
        field("fields");
        inspectNamedNodeArray(node->as.type_decl.fields, depth);
        break;

    case Node_member:
        field("lhs");
        inspectNode(node->as.member.lhs);
        field("name");
        inspectId(node->as.member.name);
        break;

    case Node_numeric_f64:
        field("value");
        ss << node->as.numeric.val.f64;
        break;

    case Node_numeric_i64:
        field("value");
        ss << node->as.numeric.val.i64;
        break;

    case Node_id:
        field("name");
        inspectId(node->as.id.name);
        break;

    case Node_call:
        field("lhs");
        inspectNode(node->as.call.lhs);
        field("args");
        inspectNodeArray(node->as.call.args, depth);
        break;

    case Node_var_decl:
        field("name");
        inspectId(node->as.var_decl.name);
        field("type");
        inspectNode(node->as.var_decl.type);
        field("value");
        inspectNode(node->as.var_decl.value);
        break;

    case Node_fn:
        field("name");
        inspectId(node->as.fn.sig.name);
        field("sig");
        inspectSig(node->as.fn.sig, depth);
        field("body");
        inspectNode(node->as.fn.body);
        break;

    case Node_string_literal:
        field("value");
        ss << "\"" << node->as.str.val << "\"";
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
