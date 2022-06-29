#include "nkl/lang/ast.hpp"

namespace nkl {

#define N(TYPE, ID)                 \
    Node LangAst::make_##ID() {     \
        return Node{{}, Node_##ID}; \
    }
#define U(TYPE, ID)                                  \
    Node LangAst::make_##ID(Node const &arg) {       \
        return Node{{push(arg), {}, {}}, Node_##ID}; \
    }
#define B(TYPE, ID)                                             \
    Node LangAst::make_##ID(Node const &lhs, Node const &rhs) { \
        return Node{{push(lhs), push(rhs), {}}, Node_##ID};     \
    }
#include "nkl/lang/nodes.inl"

/// @TODO Actually used Id's
Node LangAst::make_if(Node const &cond, Node const &then_clause, Node const &else_clause) {
    return Node{{push(cond), push(then_clause), push(else_clause)}, Node_if};
}

} // namespace nkl
