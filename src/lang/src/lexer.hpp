#ifndef HEADER_GUARD_NKL_LANG_LEXER
#define HEADER_GUARD_NKL_LANG_LEXER

#include <vector>

#include "nk/common/string.h"
#include "nkl/lang/token.h"

std::vector<NklToken> nkl_lex(nkstr filename);

#endif // HEADER_GUARD_NKL_LANG_LEXER
