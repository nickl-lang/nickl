#include "nkl/lang/ast.hpp"

namespace nkl {

Node LangAst::make_if(Node const &cond, Node const &then_clause, Node const &else_clause) {
    return Node{{push(cond), push(then_clause), push(else_clause)}, Node_if};
}

} // namespace nkl
