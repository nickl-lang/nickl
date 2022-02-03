#ifndef HEADER_GUARD_NKL_CORE_AST
#define HEADER_GUARD_NKL_CORE_AST

#include "nk/common/id.hpp"
#include "nk/common/sequence.hpp"
#include "nkl/core/common.hpp"

enum ENodeId {
#define X(TYPE, ID) Node_##ID,
#include "nkl/core/nodes.inl"

    Node_count,
};

extern char const *s_node_names[];

struct Node;
using node_ref_t = Node const *;

struct NamedNode {
    Id name;
    node_ref_t node;
};

using NodeArray = Slice<Node const>;
using NamedNodeArray = Slice<NamedNode const>;

struct _null {
    uint8_t _dummy;
};

struct _array {
    NodeArray nodes;
};

struct _unary {
    node_ref_t arg;
};

struct _binary {
    node_ref_t lhs;
    node_ref_t rhs;
};

struct _ternary {
    node_ref_t arg1;
    node_ref_t arg2;
    node_ref_t arg3;
};

struct _id {
    Id name;
};

struct _type_decl {
    Id name;
    NodeArray fields;
};

struct _call {
    node_ref_t lhs;
    NodeArray args;
};

struct _fn_type {
    Id name;
    NodeArray params;
    node_ref_t ret_type;
};

struct _fn {
    struct _fn_type sig;
    node_ref_t body;
};

struct _member {
    node_ref_t lhs;
    Id name;
};

struct _numeric {
    NumericVariant val;
};

struct _str {
    string val;
};

struct _struct_literal {
    node_ref_t type;
    NodeArray fields;
};

struct _var_decl {
    Id name;
    node_ref_t type;
    node_ref_t value;
};

struct Node {
    union {
        struct _null null;

        struct _array array;
        struct _binary binary;
        struct _call call;
        struct _fn fn;
        struct _fn_type fn_type;
        struct _id id;
        struct _member member;
        struct _numeric numeric;
        struct _str str;
        struct _struct_literal struct_literal;
        struct _ternary ternary;
        struct _type_decl type_decl;
        struct _unary unary;
        struct _var_decl var_decl;

        NamedNode named_node;
    } as;
    Id id;
};

struct Ast {
    Sequence<Node> data;

    void init();
    void deinit();

#define NULLARY_NODE(NAME) Node make_##NAME();

    NULLARY_NODE(any)
    NULLARY_NODE(bool)
    NULLARY_NODE(break)
    NULLARY_NODE(continue)
    NULLARY_NODE(f32)
    NULLARY_NODE(f64)
    NULLARY_NODE(false)
    NULLARY_NODE(i16)
    NULLARY_NODE(i32)
    NULLARY_NODE(i64)
    NULLARY_NODE(i8)
    NULLARY_NODE(nop)
    NULLARY_NODE(string)
    NULLARY_NODE(symbol)
    NULLARY_NODE(true)
    NULLARY_NODE(typeref)
    NULLARY_NODE(u16)
    NULLARY_NODE(u32)
    NULLARY_NODE(u64)
    NULLARY_NODE(u8)
    NULLARY_NODE(void)

#undef NULLARY_NODE

#define UNARY_NODE(NAME) Node make_##NAME(node_ref_t arg);

    UNARY_NODE(addr)
    UNARY_NODE(alignof)
    UNARY_NODE(assert)
    UNARY_NODE(compl )
    UNARY_NODE(const)
    UNARY_NODE(dec)
    UNARY_NODE(deref)
    UNARY_NODE(inc)
    UNARY_NODE(not )
    UNARY_NODE(pdec)
    UNARY_NODE(pinc)
    UNARY_NODE(return )
    UNARY_NODE(sizeof)
    UNARY_NODE(typeof)
    UNARY_NODE(uminus)
    UNARY_NODE(uplus)

#undef UNARY_NODE

#define BINARY_NODE(NAME) Node make_##NAME(node_ref_t lhs, node_ref_t rhs);

    BINARY_NODE(add)
    BINARY_NODE(add_assign)
    BINARY_NODE(and)
    BINARY_NODE(and_assign)
    BINARY_NODE(array_type)
    BINARY_NODE(assign)
    BINARY_NODE(bitand)
    BINARY_NODE(bitand_assign)
    BINARY_NODE(bitor)
    BINARY_NODE(bitor_assign)
    BINARY_NODE(cast)
    BINARY_NODE(colon_assign)
    BINARY_NODE(div)
    BINARY_NODE(div_assign)
    BINARY_NODE(eq)
    BINARY_NODE(ge)
    BINARY_NODE(gt)
    BINARY_NODE(index)
    BINARY_NODE(le)
    BINARY_NODE(lsh)
    BINARY_NODE(lsh_assign)
    BINARY_NODE(lt)
    BINARY_NODE(mod)
    BINARY_NODE(mod_assign)
    BINARY_NODE(mul)
    BINARY_NODE(mul_assign)
    BINARY_NODE(ne)
    BINARY_NODE(or)
    BINARY_NODE(or_assign)
    BINARY_NODE(rsh)
    BINARY_NODE(rsh_assign)
    BINARY_NODE(sub)
    BINARY_NODE(sub_assign)
    BINARY_NODE(while)
    BINARY_NODE(xor)
    BINARY_NODE(xor_assign)

#undef BINARY_NODE

    Node make_if(node_ref_t cond, node_ref_t then_clause, node_ref_t else_clause);
    Node make_ternary(node_ref_t cond, node_ref_t then_clause, node_ref_t else_clause);

    Node make_array(NodeArray nodes);
    Node make_block(NodeArray nodes);
    Node make_tuple(NodeArray nodes);
    Node make_tuple_type(NodeArray nodes);

    Node make_id(Id name);
    Node make_sym(Id name);

    Node make_member(node_ref_t lhs, Id name);
    Node make_offsetof(node_ref_t lhs, Id name);

    Node make_numeric_i8(int8_t val);
    Node make_numeric_i16(int16_t val);
    Node make_numeric_i32(int32_t val);
    Node make_numeric_i64(int64_t val);
    Node make_numeric_u8(uint8_t val);
    Node make_numeric_u16(uint16_t val);
    Node make_numeric_u32(uint32_t val);
    Node make_numeric_u64(uint64_t val);
    Node make_numeric_f32(float val);
    Node make_numeric_f64(double val);

    Node make_struct(Id name, NamedNodeArray fields);
    Node make_union(Id name, NamedNodeArray fields);

    Node make_call(node_ref_t lhs, NodeArray args);
    Node make_fn(Id name, NamedNodeArray params, node_ref_t ret_type, node_ref_t body);
    Node make_fn_type(Id name, NamedNodeArray params, node_ref_t ret_type);
    Node make_string_literal(Allocator *allocator, string str);
    Node make_struct_literal(node_ref_t type, NamedNodeArray fields);
    Node make_var_decl(Id name, node_ref_t type, node_ref_t value);

    node_ref_t push(Node node);
    NodeArray push_ar(NodeArray nodes);
    NodeArray push_named_ar(NamedNodeArray nodes);
};

string ast_inspect(Allocator *allocator, node_ref_t node);

static node_ref_t const n_none_ref = nullptr;

#endif // HEADER_GUARD_NKL_CORE_AST
