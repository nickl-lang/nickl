#include "lexer.h"

NkIrLexerResult nkir_lex(NkAllocator alloc, nkstr src) {
    NkIrLexerResult res{
        .tokens{},
        .error_msg{},
        .ok = true,
    };
    return res;
}
