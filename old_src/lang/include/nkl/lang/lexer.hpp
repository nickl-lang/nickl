#ifndef HEADER_GUARD_NKL_LANG_LEXER
#define HEADER_GUARD_NKL_LANG_LEXER

#include "nk/ds/array.hpp"
#include "nk/str/string_builder.hpp"
#include "nkl/core/token.hpp"

namespace nkl {

struct Lexer {
    StringBuilder &err;
    Array<Token> tokens{};

    bool lex(string text);
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_LEXER
