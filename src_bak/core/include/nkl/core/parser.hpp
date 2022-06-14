#ifndef HEADER_GUARD_NKL_CORE_PARSER
#define HEADER_GUARD_NKL_CORE_PARSER

#include "nkl/core/ast.hpp"
#include "nkl/core/token.hpp"

namespace nkl {

struct Parser {
    Ast ast;
    node_ref_t root;
    string err;

    bool parse(TokenArray const &tokens);
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_PARSER
