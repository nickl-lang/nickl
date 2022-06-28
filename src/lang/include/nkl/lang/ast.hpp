#ifndef HEADER_GUARD_NKL_LANG_AST
#define HEADER_GUARD_NKL_LANG_AST

#include "nkl/core/ast.hpp"

namespace nkl {

enum ENodeId {
    Node_if,
};

struct LangAst : Ast {
    Node make_if(Node const &cond, Node const &then_clause, Node const &else_clause);

private:
    using Ast::push;
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_AST
