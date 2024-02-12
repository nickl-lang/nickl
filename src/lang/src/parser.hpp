#ifndef NKL_LANG_PARSER_HPP_
#define NKL_LANG_PARSER_HPP_

#include <string>

#include "nkl/lang/ast.h"
#include "nkl/lang/token.h"

NklAstNode nkl_parse(NklAst ast, NklTokenArray tokens, std::string &err_str, NklTokenRef &err_token);

#endif // NKL_LANG_PARSER_HPP_
