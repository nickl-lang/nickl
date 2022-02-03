#include "nkl/core/ast.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iterator>
#include <sstream>
#include <string>

#define AST_INIT_CAPACITY 1024

char const *s_node_names[] = {
#define X(TYPE, ID) #ID,
#include "nkl/core//nodes.inl"
};

void Ast::init() {
    data.init(AST_INIT_CAPACITY);
}

void Ast::deinit() {
    data.deinit();
}

Node Ast::make_any() {
    return Node{{.null = {0}}, Node_any};
}

Node Ast::make_bool() {
    return Node{{.null = {0}}, Node_bool};
}

Node Ast::make_break() {
    return Node{{.null = {0}}, Node_break};
}

Node Ast::make_continue() {
    return Node{{.null = {0}}, Node_continue};
}

Node Ast::make_f32() {
    return Node{{.null = {0}}, Node_f32};
}

Node Ast::make_f64() {
    return Node{{.null = {0}}, Node_f64};
}

Node Ast::make_false() {
    return Node{{.null = {0}}, Node_false};
}

Node Ast::make_i16() {
    return Node{{.null = {0}}, Node_i16};
}

Node Ast::make_i32() {
    return Node{{.null = {0}}, Node_i32};
}

Node Ast::make_i64() {
    return Node{{.null = {0}}, Node_i64};
}

Node Ast::make_i8() {
    return Node{{.null = {0}}, Node_i8};
}

Node Ast::make_nop() {
    return Node{{.null = {0}}, Node_nop};
}

Node Ast::make_string() {
    return Node{{.null = {0}}, Node_string};
}

Node Ast::make_symbol() {
    return Node{{.null = {0}}, Node_symbol};
}

Node Ast::make_true() {
    return Node{{.null = {0}}, Node_true};
}

Node Ast::make_typeref() {
    return Node{{.null = {0}}, Node_typeref};
}

Node Ast::make_u16() {
    return Node{{.null = {0}}, Node_u16};
}

Node Ast::make_u32() {
    return Node{{.null = {0}}, Node_u32};
}

Node Ast::make_u64() {
    return Node{{.null = {0}}, Node_u64};
}

Node Ast::make_u8() {
    return Node{{.null = {0}}, Node_u8};
}

Node Ast::make_void() {
    return Node{{.null = {0}}, Node_void};
}

Node Ast::make_addr(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_addr};
}

Node Ast::make_alignof(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_alignof};
}

Node Ast::make_assert(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_assert};
}

Node Ast::make_compl(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_compl};
}

Node Ast::make_const(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_const};
}

Node Ast::make_dec(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_dec};
}

Node Ast::make_deref(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_deref};
}

Node Ast::make_inc(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_inc};
}

Node Ast::make_not(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_not};
}

Node Ast::make_pdec(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_pdec};
}

Node Ast::make_pinc(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_pinc};
}

Node Ast::make_return(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_return};
}

Node Ast::make_sizeof(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_sizeof};
}

Node Ast::make_typeof(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_typeof};
}

Node Ast::make_uminus(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_uminus};
}

Node Ast::make_uplus(node_ref_t arg) {
    return Node{{.unary = {arg}}, Node_uplus};
}

Node Ast::make_add(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_add};
}

Node Ast::make_add_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_add_assign};
}

Node Ast::make_and(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_and};
}

Node Ast::make_and_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_and_assign};
}

Node Ast::make_array_type(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_array_type};
}

Node Ast::make_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_assign};
}

Node Ast::make_bitand(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_bitand};
}

Node Ast::make_bitand_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_bitand_assign};
}

Node Ast::make_bitor(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_bitor};
}

Node Ast::make_bitor_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_bitor_assign};
}

Node Ast::make_cast(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_cast};
}

Node Ast::make_colon_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_colon_assign};
}

Node Ast::make_div(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_div};
}

Node Ast::make_div_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_div_assign};
}

Node Ast::make_eq(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_eq};
}

Node Ast::make_ge(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_ge};
}

Node Ast::make_gt(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_gt};
}

Node Ast::make_index(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_index};
}

Node Ast::make_le(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_le};
}

Node Ast::make_lsh(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_lsh};
}

Node Ast::make_lsh_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_lsh_assign};
}

Node Ast::make_lt(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_lt};
}

Node Ast::make_mod(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_mod};
}

Node Ast::make_mod_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_mod_assign};
}

Node Ast::make_mul(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_mul};
}

Node Ast::make_mul_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_mul_assign};
}

Node Ast::make_ne(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_ne};
}

Node Ast::make_or(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_or};
}

Node Ast::make_or_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_or_assign};
}

Node Ast::make_rsh(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_rsh};
}

Node Ast::make_rsh_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_rsh_assign};
}

Node Ast::make_sub(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_sub};
}

Node Ast::make_sub_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_sub_assign};
}

Node Ast::make_while(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_while};
}

Node Ast::make_xor(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_xor};
}

Node Ast::make_xor_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, Node_xor_assign};
}

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

Node Ast::make_sym(Id name) {
    return Node{{.id = {name}}, Node_sym};
}

Node Ast::make_member(node_ref_t lhs, Id name) {
    return Node{{.member = {lhs, name}}, Node_member};
}

Node Ast::make_offsetof(node_ref_t lhs, Id name) {
    return Node{{.member = {lhs, name}}, Node_offsetof};
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

Node Ast::make_union(Id name, NamedNodeArray fields) {
    return Node{{.type_decl = {name, push_named_ar(fields)}}, Node_union};
}

Node Ast::make_call(node_ref_t lhs, NodeArray args) {
    return Node{{.call = {lhs, push_ar(args)}}, Node_call};
}

Node Ast::make_fn(Id name, NamedNodeArray params, node_ref_t ret_type, node_ref_t body) {
    return Node{{.fn = {{name, push_named_ar(params), ret_type}, body}}, Node_fn};
}

Node Ast::make_fn_type(Id name, NamedNodeArray params, node_ref_t ret_type) {
    return Node{{.fn_type = {name, push_named_ar(params), ret_type}}, Node_fn_type};
}

Node Ast::make_string_literal(Allocator *allocator, string str) {
    char *ast_str = allocator->alloc<char>(str.size);
    std::strncpy(ast_str, str.data, str.size);
    return Node{{.str = {{ast_str, str.size}}}, Node_string_literal};
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

std::string _inspect(node_ref_t node, size_t depth = 1) {
    assert(depth > 0);

    std::ostringstream ss;

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
            ss << _inspect(ar.data + i, depth + 1);
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
            ss << "name: #" << id_to_str(node->as.named_node.name) << ",";
            newline(depth + 1);
            ss << "node: " << _inspect(node->as.named_node.node, depth + 2) << "}";
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
        ss << "ret_type: " << _inspect(sig.ret_type, depth + 2) << "}";
    };

    auto const inspectNode = [&](node_ref_t node) {
        ss << _inspect(node, depth + 1);
    };

    auto const inspectId = [&](Id id) {
        if (id) {
            ss << "#" << id_to_str(id);
        } else {
            ss << "null";
        }
    };

    if (!node) {
        ss << "null";
        return ss.str();
    }

    ss << "{";
    newline(depth);
    ss << "id: " << s_node_names[node->id];

    switch (node->id) {
    default:
        break;

    case Node_addr:
    case Node_alignof:
    case Node_assert:
    case Node_compl:
    case Node_const:
    case Node_dec:
    case Node_deref:
    case Node_inc:
    case Node_not:
    case Node_pdec:
    case Node_pinc:
    case Node_return:
    case Node_sizeof:
    case Node_typeof:
    case Node_uminus:
    case Node_uplus:
        field("arg");
        inspectNode(node->as.unary.arg);
        break;

    case Node_add:
    case Node_add_assign:
    case Node_and:
    case Node_and_assign:
    case Node_array_type:
    case Node_assign:
    case Node_bitand:
    case Node_bitor:
    case Node_cast:
    case Node_colon_assign:
    case Node_div:
    case Node_div_assign:
    case Node_eq:
    case Node_ge:
    case Node_gt:
    case Node_index:
    case Node_le:
    case Node_lsh:
    case Node_lsh_assign:
    case Node_lt:
    case Node_mod:
    case Node_mod_assign:
    case Node_mul:
    case Node_mul_assign:
    case Node_ne:
    case Node_or:
    case Node_or_assign:
    case Node_rsh:
    case Node_rsh_assign:
    case Node_sub:
    case Node_sub_assign:
    case Node_while:
    case Node_xor:
    case Node_xor_assign:
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
    case Node_union:
        field("name");
        inspectId(node->as.type_decl.name);
        field("fields");
        inspectNamedNodeArray(node->as.type_decl.fields, depth);
        break;

    case Node_member:
    case Node_offsetof:
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
    case Node_sym:
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

    case Node_fn_type:
        field("name");
        inspectId(node->as.fn_type.name);
        field("sig");
        inspectSig(node->as.fn_type, depth);
        break;
    }

    ss << "}";

    return ss.str();
}

} // namespace

string ast_inspect(Allocator *allocator, node_ref_t node) {
    auto str = _inspect(node);
    char *data = allocator->alloc<char>(str.size());
    std::strncpy(data, str.c_str(), str.size());
    return string{data, str.size()};
}
