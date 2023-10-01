#ifndef HEADER_GUARD_NKL_LANG_LEXER
#define HEADER_GUARD_NKL_LANG_LEXER

#include <string>
#include <vector>

#include "nkl/lang/token.h"
#include "ntk/string.h"

bool nkl_lex(nkstr src, std::vector<NklToken> &tokens, std::string &err_str);

#endif // HEADER_GUARD_NKL_LANG_LEXER
