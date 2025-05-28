#ifndef NKL_CORE_AST_TOKENS_H_
#define NKL_CORE_AST_TOKENS_H_

#include "nkl/core/lexer.h"

enum {
    NklAstToken_OperatorsBase = NklToken_Count,

    NklAstToken_LParen,
    NklAstToken_RParen,
    NklAstToken_LBrace,
    NklAstToken_RBrace,
    NklAstToken_LBraket,
    NklAstToken_RBraket,
};

#endif // NKL_CORE_AST_TOKENS_H_
