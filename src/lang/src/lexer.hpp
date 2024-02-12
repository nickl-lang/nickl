#ifndef NKL_LANG_LEXER_HPP_
#define NKL_LANG_LEXER_HPP_

#include <string>
#include <vector>

#include "nkl/lang/token.h"
#include "ntk/string.h"

bool nkl_lex(NkString src, std::vector<NklToken> &tokens, std::string &err_str);

#endif // NKL_LANG_LEXER_HPP_
