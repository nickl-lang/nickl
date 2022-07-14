#ifndef HEADER_GUARD_NKL_LANG_AST
#define HEADER_GUARD_NKL_LANG_AST

#include "nk/mem/stack_allocator.hpp"
#include "nk/utils/utils.hpp"
#include "nkl/core/ast.hpp"

namespace nkl {

enum ENodeId {
#define X(ID) CAT(Node_, ID),
#include "nkl/lang/nodes.inl"

    Node_count,
};

struct NamedNode {
    TokenRef name;
    NodeRef node;
};

using NamedNodeArray = Slice<NamedNode const>;

struct FieldNode {
    TokenRef name;
    NodeRef type;
    NodeRef init_value;
    bool is_const;
};

using FieldNodeArray = Slice<FieldNode const>;

Id nodeId(ENodeId id);

struct LangAst : Ast {
    void init();
    void deinit();

#define N(ID) Node CAT(make_, ID)();
#define U(ID) Node CAT(make_, ID)(Node const &arg);
#define B(ID) Node CAT(make_, ID)(Node const &lhs, Node const &rhs);
#include "nkl/lang/nodes.inl"

    Node make_if(Node const &cond, Node const &then_clause, Node const &else_clause);
    Node make_ternary(Node const &cond, Node const &then_clause, Node const &else_clause);

    Node make_array(NodeArray nodes);
    Node make_block(NodeArray nodes);
    Node make_tuple(NodeArray nodes);
    Node make_tuple_type(NodeArray nodes);

    Node make_id(TokenRef name);
    Node make_numeric_float(TokenRef val);
    Node make_numeric_int(TokenRef val);
    Node make_string_literal(TokenRef val);
    Node make_escaped_string_literal(TokenRef val);
    Node make_import_path(TokenRef path);

    Node make_import(TokenRefArray names);

    Node make_for(TokenRef it, Node const &range, Node const &body);
    Node make_for_by_ptr(TokenRef it, Node const &range, Node const &body);

    Node make_member(Node const &lhs, TokenRef name);

    Node make_struct(FieldNodeArray fields);
    Node make_union(FieldNodeArray fields);
    Node make_enum(FieldNodeArray fields);
    Node make_packed_struct(FieldNodeArray fields);

    Node make_fn(
        FieldNodeArray params,
        Node const &ret_type,
        Node const &body,
        bool is_variadic = false);

    Node make_tag(TokenRef tag, NamedNodeArray args, Node const &node);

    Node make_call(Node const &lhs, NamedNodeArray args);
    Node make_object_literal(Node const &lhs, NamedNodeArray args);

    Node make_assign(NodeArray lhs, Node const &value);
    Node make_define(TokenRefArray names, Node const &value);

    Node make_comptime_const_def(TokenRef name, Node const &value);
    Node make_tag_def(TokenRef name, Node const &type);

    Node make_var_decl(TokenRef name, Node const &type, Node const &value);
    Node make_const_decl(TokenRef name, Node const &type, Node const &value);

private:
    NodeArg push(NamedNodeArray nns);
    NodeArg push(FieldNodeArray fields);
    NodeArg push(TokenRefArray tokens);

    using Ast::push;

    StackAllocator m_arena{};
};

struct PackedNamedNodeArray : PackedNodeArgArray {
    using PackedNodeArgArray::PackedNodeArgArray;
    NamedNode operator[](size_t i) const;
};

struct PackedFieldNodeArray : NodeArray {
    PackedFieldNodeArray(NodeArray fields);
    FieldNode operator[](size_t i) const;
};

struct PackedTokenRefArray : PackedNodeArgArray {
    using PackedNodeArgArray::PackedNodeArgArray;
    TokenRef operator[](size_t i) const;
};

} // namespace nkl

#define _NodeArg(NODE, IDX) (NODE)->arg[IDX]

#define _NodeArgAsToken(NODE, IDX) (_NodeArg(NODE, IDX).token)
#define _NodeArgAsNode(NODE, IDX) (_NodeArg(NODE, IDX).nodes.begin())
#define _NodeArgAsNodeAr(NODE, IDX) (_NodeArg(NODE, IDX).nodes)
#define _NodeArgAsNamedNodeAr(NODE, IDX) (PackedNamedNodeArray{_NodeArg(NODE, IDX).nodes})
#define _NodeArgAsFieldAr(NODE, IDX) (PackedFieldNodeArray{_NodeArg(NODE, IDX).nodes})
#define _NodeArgAsTokenAr(NODE, IDX) (PackedTokenRefArray{_NodeArg(NODE, IDX).nodes})

#define Node_unary_arg(NODE) _NodeArgAsNode((NODE), 0)

#define Node_binary_lhs(NODE) _NodeArgAsNode((NODE), 0)
#define Node_binary_rhs(NODE) _NodeArgAsNode((NODE), 1)

#define Node_ternary_cond(NODE) _NodeArgAsNode((NODE), 0)
#define Node_ternary_then_clause(NODE) _NodeArgAsNode((NODE), 1)
#define Node_ternary_else_clause(NODE) _NodeArgAsNode((NODE), 2)

#define Node_array_nodes(NODE) _NodeArgAsNodeAr((NODE), 0)

#define Node_import_names(NODE) _NodeArgAsTokenAr((NODE), 0)

#define Node_token_value(NODE) _NodeArgAsToken((NODE), 0)

#define Node_for_it(NODE) _NodeArgAsToken((NODE), 0)
#define Node_for_range(NODE) _NodeArgAsNode((NODE), 1)
#define Node_for_body(NODE) _NodeArgAsNode((NODE), 2)

#define Node_member_lhs(NODE) _NodeArgAsNode((NODE), 0)
#define Node_member_name(NODE) _NodeArgAsToken((NODE), 1)

#define Node_type_fields(NODE) _NodeArgAsFieldAr((NODE), 0)

#define Node_fn_params(NODE) _NodeArgAsFieldAr((NODE), 0)
#define Node_fn_ret_type(NODE) _NodeArgAsNode((NODE), 1)
#define Node_fn_body(NODE) _NodeArgAsNode((NODE), 2)

#define Node_tag_tag(NODE) _NodeArgAsToken((NODE), 0)
#define Node_tag_args(NODE) _NodeArgAsNamedNodeAr((NODE), 1)
#define Node_tag_node(NODE) _NodeArgAsNode((NODE), 2)

#define Node_call_lhs(NODE) _NodeArgAsNode((NODE), 0)
#define Node_call_args(NODE) _NodeArgAsNamedNodeAr((NODE), 1)

#define Node_assign_lhs(NODE) _NodeArgAsNodeAr((NODE), 0)
#define Node_assign_value(NODE) _NodeArgAsNode((NODE), 1)

#define Node_define_names(NODE) _NodeArgAsTokenAr((NODE), 0)
#define Node_define_value(NODE) _NodeArgAsNode((NODE), 1)

#define Node_comptime_const_def_name(NODE) _NodeArgAsToken((NODE), 0)
#define Node_comptime_const_def_value(NODE) _NodeArgAsNode((NODE), 1)

#define Node_var_decl_name(NODE) _NodeArgAsToken((NODE), 0)
#define Node_var_decl_type(NODE) _NodeArgAsNode((NODE), 1)
#define Node_var_decl_value(NODE) _NodeArgAsNode((NODE), 2)

#endif // HEADER_GUARD_NKL_LANG_AST
