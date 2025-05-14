#ifndef NKL_CORE_IR_TOKENS_H_
#define NKL_CORE_IR_TOKENS_H_

#include "nkl/core/lexer.h"

enum {
    NklIrToken_KeywordsBase = NklToken_Count,

    NklIrToken_Pub,
    NklIrToken_Proc,
    NklIrToken_I32,
    NklIrToken_Ret,

    NklIrToken_OperatorsBase,

    NklIrToken_LParen,
    NklIrToken_RParen,
    NklIrToken_LBrace,
    NklIrToken_RBrace,

    NklIrToken_TagsBase,

    NklIrToken_AtTag,
};

#endif // NKL_CORE_IR_TOKENS_H_
