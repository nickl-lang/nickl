#ifndef HEADER_GUARD_NKL_LANG_PARSER
#define HEADER_GUARD_NKL_LANG_PARSER

#include "nk/common/slice.hpp"
#include "nkl/lang/ast.h"
#include "nkl/lang/token.h"

NklAstNode nkl_parse(NklAst ast, nkslice<NklToken const> tokens);

#endif // HEADER_GUARD_NKL_LANG_PARSER
