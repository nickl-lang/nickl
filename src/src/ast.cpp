#include "nkl/core/ast.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iterator>
#include <sstream>
#include <string>

#define AST_INIT_CAPACITY 1024

void Ast::init() {
    data.init(AST_INIT_CAPACITY);
}

void Ast::deinit() {
    data.deinit();
}

Node Ast::make_any() {
    return Node{{.null = {0}}, id_any};
}

Node Ast::make_bool() {
    return Node{{.null = {0}}, id_bool};
}

Node Ast::make_break() {
    return Node{{.null = {0}}, id_break};
}

Node Ast::make_continue() {
    return Node{{.null = {0}}, id_continue};
}

Node Ast::make_f32() {
    return Node{{.null = {0}}, id_f32};
}

Node Ast::make_f64() {
    return Node{{.null = {0}}, id_f64};
}

Node Ast::make_false() {
    return Node{{.null = {0}}, id_false};
}

Node Ast::make_i16() {
    return Node{{.null = {0}}, id_i16};
}

Node Ast::make_i32() {
    return Node{{.null = {0}}, id_i32};
}

Node Ast::make_i64() {
    return Node{{.null = {0}}, id_i64};
}

Node Ast::make_i8() {
    return Node{{.null = {0}}, id_i8};
}

Node Ast::make_nop() {
    return Node{{.null = {0}}, id_nop};
}

Node Ast::make_string() {
    return Node{{.null = {0}}, id_string};
}

Node Ast::make_symbol() {
    return Node{{.null = {0}}, id_symbol};
}

Node Ast::make_true() {
    return Node{{.null = {0}}, id_true};
}

Node Ast::make_typeref() {
    return Node{{.null = {0}}, id_typeref};
}

Node Ast::make_u16() {
    return Node{{.null = {0}}, id_u16};
}

Node Ast::make_u32() {
    return Node{{.null = {0}}, id_u32};
}

Node Ast::make_u64() {
    return Node{{.null = {0}}, id_u64};
}

Node Ast::make_u8() {
    return Node{{.null = {0}}, id_u8};
}

Node Ast::make_void() {
    return Node{{.null = {0}}, id_void};
}

Node Ast::make_addr(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_addr};
}

Node Ast::make_alignof(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_alignof};
}

Node Ast::make_assert(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_assert};
}

Node Ast::make_compl(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_compl};
}

Node Ast::make_const(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_const};
}

Node Ast::make_dec(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_dec};
}

Node Ast::make_deref(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_deref};
}

Node Ast::make_inc(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_inc};
}

Node Ast::make_not(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_not};
}

Node Ast::make_pdec(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_pdec};
}

Node Ast::make_pinc(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_pinc};
}

Node Ast::make_return(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_return};
}

Node Ast::make_sizeof(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_sizeof};
}

Node Ast::make_typeof(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_typeof};
}

Node Ast::make_uminus(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_uminus};
}

Node Ast::make_uplus(node_ref_t arg) {
    return Node{{.unary = {arg}}, id_uplus};
}

Node Ast::make_add(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_add};
}

Node Ast::make_add_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_add_assign};
}

Node Ast::make_and(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_and};
}

Node Ast::make_and_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_and_assign};
}

Node Ast::make_array_type(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_array_type};
}

Node Ast::make_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_assign};
}

Node Ast::make_bitand(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_bitand};
}

Node Ast::make_bitand_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_bitand_assign};
}

Node Ast::make_bitor(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_bitor};
}

Node Ast::make_bitor_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_bitor_assign};
}

Node Ast::make_cast(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_cast};
}

Node Ast::make_colon_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_colon_assign};
}

Node Ast::make_div(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_div};
}

Node Ast::make_div_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_div_assign};
}

Node Ast::make_eq(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_eq};
}

Node Ast::make_ge(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_ge};
}

Node Ast::make_gt(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_gt};
}

Node Ast::make_index(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_index};
}

Node Ast::make_le(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_le};
}

Node Ast::make_lsh(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_lsh};
}

Node Ast::make_lsh_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_lsh_assign};
}

Node Ast::make_lt(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_lt};
}

Node Ast::make_mod(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_mod};
}

Node Ast::make_mod_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_mod_assign};
}

Node Ast::make_mul(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_mul};
}

Node Ast::make_mul_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_mul_assign};
}

Node Ast::make_ne(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_ne};
}

Node Ast::make_or(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_or};
}

Node Ast::make_or_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_or_assign};
}

Node Ast::make_rsh(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_rsh};
}

Node Ast::make_rsh_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_rsh_assign};
}

Node Ast::make_sub(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_sub};
}

Node Ast::make_sub_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_sub_assign};
}

Node Ast::make_while(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_while};
}

Node Ast::make_xor(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_xor};
}

Node Ast::make_xor_assign(node_ref_t lhs, node_ref_t rhs) {
    return Node{{.binary = {lhs, rhs}}, id_xor_assign};
}

Node Ast::make_if(node_ref_t cond, node_ref_t then_clause, node_ref_t else_clause) {
    return Node{{.ternary = {cond, then_clause, else_clause}}, id_if};
}

Node Ast::make_ternary(node_ref_t cond, node_ref_t then_clause, node_ref_t else_clause) {
    return Node{{.ternary = {cond, then_clause, else_clause}}, id_ternary};
}

Node Ast::make_array(NodeArray nodes) {
    return Node{{.array = {push_ar(nodes)}}, id_array};
}

Node Ast::make_block(NodeArray nodes) {
    return Node{{.array = {push_ar(nodes)}}, id_block};
}

Node Ast::make_tuple(NodeArray nodes) {
    return Node{{.array = {push_ar(nodes)}}, id_tuple};
}

Node Ast::make_tuple_type(NodeArray nodes) {
    return Node{{.array = {push_ar(nodes)}}, id_tuple_type};
}

Node Ast::make_id(Id name) {
    return Node{{.id = {name}}, id_id};
}

Node Ast::make_sym(Id name) {
    return Node{{.id = {name}}, id_sym};
}

Node Ast::make_member(node_ref_t lhs, Id name) {
    return Node{{.member = {lhs, name}}, id_member};
}

Node Ast::make_offsetof(node_ref_t lhs, Id name) {
    return Node{{.member = {lhs, name}}, id_offsetof};
}

Node Ast::make_numeric_i8(int8_t val) {
    return Node{{.numeric = {.val = {.i8 = val}}}, id_numeric_i8};
}

Node Ast::make_numeric_i16(int16_t val) {
    return Node{{.numeric = {.val = {.i16 = val}}}, id_numeric_i16};
}

Node Ast::make_numeric_i32(int32_t val) {
    return Node{{.numeric = {.val = {.i32 = val}}}, id_numeric_i32};
}

Node Ast::make_numeric_i64(int64_t val) {
    return Node{{.numeric = {.val = {.i64 = val}}}, id_numeric_i64};
}

Node Ast::make_numeric_u8(uint8_t val) {
    return Node{{.numeric = {.val = {.u8 = val}}}, id_numeric_u8};
}

Node Ast::make_numeric_u16(uint16_t val) {
    return Node{{.numeric = {.val = {.u16 = val}}}, id_numeric_u16};
}

Node Ast::make_numeric_u32(uint32_t val) {
    return Node{{.numeric = {.val = {.u32 = val}}}, id_numeric_u32};
}

Node Ast::make_numeric_u64(uint64_t val) {
    return Node{{.numeric = {.val = {.u64 = val}}}, id_numeric_u64};
}

Node Ast::make_numeric_f32(float val) {
    return Node{{.numeric = {.val = {.f32 = val}}}, id_numeric_f32};
}

Node Ast::make_numeric_f64(double val) {
    return Node{{.numeric = {.val = {.f64 = val}}}, id_numeric_f64};
}

Node Ast::make_struct(Id name, NamedNodeArray fields) {
    return Node{{.type_decl = {name, push_named_ar(fields)}}, id_struct};
}

Node Ast::make_union(Id name, NamedNodeArray fields) {
    return Node{{.type_decl = {name, push_named_ar(fields)}}, id_union};
}

Node Ast::make_call(node_ref_t lhs, NodeArray args) {
    return Node{{.call = {lhs, push_ar(args)}}, id_call};
}

Node Ast::make_fn(Id name, NamedNodeArray params, node_ref_t ret_type, node_ref_t body) {
    return Node{{.fn = {{name, push_named_ar(params), ret_type}, body}}, id_fn};
}

Node Ast::make_fn_type(Id name, NamedNodeArray params, node_ref_t ret_type) {
    return Node{{.fn_type = {name, push_named_ar(params), ret_type}}, id_fn_type};
}

Node Ast::make_string_literal(Allocator *allocator, string str) {
    char *ast_str = allocator->alloc<char>(str.size);
    std::strncpy(ast_str, str.data, str.size);
    return Node{{.str = {{str.size, ast_str}}}, id_string_literal};
}

Node Ast::make_struct_literal(node_ref_t type, NamedNodeArray fields) {
    return Node{{.struct_literal = {type, push_named_ar(fields)}}, id_struct_literal};
}

Node Ast::make_var_decl(Id name, node_ref_t type, node_ref_t value) {
    return Node{{.var_decl = {name, type, value}}, id_var_decl};
}

node_ref_t Ast::push(Node node) {
    Node *ref = &data.push();
    *ref = node;
    return ref;
}

NodeArray Ast::push_ar(NodeArray nodes) {
    Node *first = &data.push(nodes.size);
    std::memcpy(first, nodes.data, nodes.size * sizeof(Node));
    return NodeArray{nodes.size, first};
}

NodeArray Ast::push_named_ar(NamedNodeArray nodes) {
    Node *first = &data.push(nodes.size);
    NodeArray const ar{nodes.size, first};
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
            ss << "name: #" << id_to_str(node->as.named_node.name).view() << ",";
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
            ss << "#" << id_to_str(id).view();
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
    ss << "id: ";
    inspectId(node->id);

    switch (node->id) {
    default:
        break;

    case id_addr:
    case id_alignof:
    case id_assert:
    case id_compl:
    case id_const:
    case id_dec:
    case id_deref:
    case id_inc:
    case id_not:
    case id_pdec:
    case id_pinc:
    case id_return:
    case id_sizeof:
    case id_typeof:
    case id_uminus:
    case id_uplus:
        field("arg");
        inspectNode(node->as.unary.arg);
        break;

    case id_add:
    case id_add_assign:
    case id_and:
    case id_and_assign:
    case id_array_type:
    case id_assign:
    case id_bitand:
    case id_bitor:
    case id_cast:
    case id_colon_assign:
    case id_div:
    case id_div_assign:
    case id_eq:
    case id_ge:
    case id_gt:
    case id_index:
    case id_le:
    case id_lsh:
    case id_lsh_assign:
    case id_lt:
    case id_mod:
    case id_mod_assign:
    case id_mul:
    case id_mul_assign:
    case id_ne:
    case id_or:
    case id_or_assign:
    case id_rsh:
    case id_rsh_assign:
    case id_sub:
    case id_sub_assign:
    case id_while:
    case id_xor:
    case id_xor_assign:
        field("lhs");
        inspectNode(node->as.binary.lhs);
        field("rhs");
        inspectNode(node->as.binary.rhs);
        break;

    case id_if:
    case id_ternary:
        field("arg1");
        inspectNode(node->as.ternary.arg1);
        field("arg2");
        inspectNode(node->as.ternary.arg2);
        field("arg3");
        inspectNode(node->as.ternary.arg3);
        break;

    case id_array:
    case id_block:
    case id_tuple:
    case id_tuple_type:
        field("nodes");
        inspectNodeArray(node->as.array.nodes, depth);
        break;

    case id_struct:
    case id_union:
        field("name");
        inspectId(node->as.type_decl.name);
        field("fields");
        inspectNamedNodeArray(node->as.type_decl.fields, depth);
        break;

    case id_member:
    case id_offsetof:
        field("lhs");
        inspectNode(node->as.member.lhs);
        field("name");
        inspectId(node->as.member.name);
        break;

    case id_numeric_f64:
        field("value");
        ss << node->as.numeric.val.f64;
        break;

    case id_numeric_i64:
        field("value");
        ss << node->as.numeric.val.i64;
        break;

    case id_id:
    case id_sym:
        field("name");
        inspectId(node->as.id.name);
        break;

    case id_call:
        field("lhs");
        inspectNode(node->as.call.lhs);
        field("args");
        inspectNodeArray(node->as.call.args, depth);
        break;

    case id_var_decl:
        field("name");
        inspectId(node->as.var_decl.name);
        field("type");
        inspectNode(node->as.var_decl.type);
        field("value");
        inspectNode(node->as.var_decl.value);
        break;

    case id_fn:
        field("name");
        inspectId(node->as.fn.sig.name);
        field("sig");
        inspectSig(node->as.fn.sig, depth);
        field("body");
        inspectNode(node->as.fn.body);
        break;

    case id_string_literal:
        field("value");
        ss << "\"" << node->as.str.val.view() << "\"";
        break;

    case id_struct_literal:
        field("type");
        inspectNode(node->as.struct_literal.type);
        field("fields");
        inspectNamedNodeArray(node->as.struct_literal.fields, depth);
        break;

    case id_fn_type:
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
    return string{str.size(), data};
}
