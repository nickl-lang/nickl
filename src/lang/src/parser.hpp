#ifndef HEADER_GUARD_NKL_LANG_PARSER
#define HEADER_GUARD_NKL_LANG_PARSER

#include "lexer.hpp"
#include "nkl/lang/ast.h"

NklAstNode nkl_parse(std::vector<NklToken> const &tokens);

#endif // HEADER_GUARD_NKL_LANG_PARSER
