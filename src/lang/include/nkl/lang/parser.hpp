#ifndef HEADER_GUARD_NKL_LANG_PARSER
#define HEADER_GUARD_NKL_LANG_PARSER

#include "nk/str/string_builder.hpp"
#include "nkl/core/token.hpp"
#include "nkl/lang/ast.hpp"

namespace nkl {

struct Parser {
    StringBuilder &err;
    LangAst ast{};
    NodeRef root{};

    bool parse(TokenArray const &tokens);
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_PARSER
