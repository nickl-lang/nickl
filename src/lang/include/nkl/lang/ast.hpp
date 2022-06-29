#ifndef HEADER_GUARD_NKL_LANG_AST
#define HEADER_GUARD_NKL_LANG_AST

#include "nkl/core/ast.hpp"

namespace nkl {

enum ENodeId {
#define X(TYPE, ID) Node_##ID,
#include "nkl/lang/nodes.inl"

    Node_count,
};

struct LangAst : Ast {
#define N(TYPE, ID) Node make_##ID();
#define U(TYPE, ID) Node make_##ID(Node const &arg);
#define B(TYPE, ID) Node make_##ID(Node const &lhs, Node const &rhs);
#include "nkl/lang/nodes.inl"

    Node make_if(Node const &cond, Node const &then_clause, Node const &else_clause);

private:
    using Ast::push;
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_AST
