#ifndef HEADER_GUARD_NKL_CORE_AST
#define HEADER_GUARD_NKL_CORE_AST

#include "nk/common/id.hpp"
#include "nk/common/sequence.hpp"
#include "nkl/core/common.hpp"
#include "nkl/core/token.hpp"

namespace nkl {

enum ENodeId {
#define X(TYPE, ID) Node_##ID,
#include "nkl/core/nodes.inl"

    Node_count,
};

extern char const *s_ast_node_names[];

struct Node;
using node_ref_t = Node const *;

struct NamedNode {
    token_ref_t name;
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

struct _token {
    token_ref_t val;
};

struct _type_decl {
    token_ref_t name;
    NodeArray fields;
};

struct _call {
    node_ref_t lhs;
    NodeArray args;
};

struct _fn_type {
    token_ref_t name;
    NodeArray params;
    node_ref_t ret_type;
};

struct _fn {
    struct _fn_type sig;
    node_ref_t body;
    token_ref_t lib;
    bool is_variadic;
};

struct _member {
    node_ref_t lhs;
    token_ref_t name;
};

struct _struct_literal {
    node_ref_t type;
    NodeArray fields;
};

struct _var_decl {
    token_ref_t name;
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
        struct _token token;
        struct _member member;
        struct _struct_literal struct_literal;
        struct _ternary ternary;
        struct _type_decl type_decl;
        struct _unary unary;
        struct _var_decl var_decl;

        NamedNode named_node;
    } as;
    size_t id;
};

struct Ast {
    Sequence<Node> data;

    void init();
    void deinit();

#define N(TYPE, ID) Node make_##ID();
#define U(TYPE, ID) Node make_##ID(Node const &arg);
#define B(TYPE, ID) Node make_##ID(Node const &lhs, Node const &rhs);
#include "nkl/core/nodes.inl"

    Node make_if(Node const &cond, Node const &then_clause, Node const &else_clause);
    Node make_ternary(Node const &cond, Node const &then_clause, Node const &else_clause);

    Node make_array(NodeArray nodes);
    Node make_block(NodeArray nodes);
    Node make_tuple(NodeArray nodes);
    Node make_id_tuple(NodeArray nodes);
    Node make_tuple_type(NodeArray nodes);

    Node make_id(token_ref_t name);
    Node make_numeric_float(token_ref_t val);
    Node make_numeric_int(token_ref_t val);
    Node make_string_literal(token_ref_t val);

    Node make_member(Node const &lhs, token_ref_t name);

    Node make_struct(token_ref_t name, NamedNodeArray fields);

    Node make_call(Node const &lhs, NodeArray args);
    Node make_fn(token_ref_t name, NamedNodeArray params, Node const &ret_type, Node const &body);
    Node make_foreign_fn(
        token_ref_t lib,
        token_ref_t name,
        NamedNodeArray params,
        Node const &ret_type,
        bool is_variadic);
    Node make_struct_literal(Node const &type, NamedNodeArray fields);
    Node make_var_decl(token_ref_t name, Node const &type, Node const &value);

    node_ref_t push(Node node);
    NodeArray push_ar(NodeArray nodes);
    NodeArray push_named_ar(NamedNodeArray nodes);
};

string ast_inspect(node_ref_t node);

static node_ref_t const n_none_ref = nullptr;

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_AST
