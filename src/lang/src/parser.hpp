#ifndef HEADER_GUARD_NKL_LANG_PARSER
#define HEADER_GUARD_NKL_LANG_PARSER

#include <string>

#include "nkl/lang/ast.h"
#include "nkl/lang/token.h"

NklAstNode nkl_parse(NklAst ast, NklTokenView tokens, std::string &err_str, NklTokenRef &err_token);

#endif // HEADER_GUARD_NKL_LANG_PARSER
