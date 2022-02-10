#ifndef HEADER_GUARD_NKL_CORE_LEXER
#define HEADER_GUARD_NKL_CORE_LEXER

#include "nk/common/array.hpp"
#include "nkl/core/token.hpp"

namespace nkl {

struct Lexer {
    Array<Token> tokens;
    string err;

    bool lex(string text);
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_LEXER
