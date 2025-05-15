#ifndef NKL_CORE_IR_TOKENS_H_
#define NKL_CORE_IR_TOKENS_H_

#include "nkb/types.h"
#include "nkl/core/lexer.h"
#include "ntk/common.h"

enum {
    NklIrToken_KeywordsBase = NklToken_Count,

    NklIrToken_pub,
    NklIrToken_proc,
    NklIrToken_cmp,
    NklIrToken_void,

#define IR(NAME) NK_CAT(NklIrToken_, NAME),
#define UNA_IR(NAME) NK_CAT(NklIrToken_, NAME),
#define BIN_IR(NAME) NK_CAT(NklIrToken_, NAME),
#define CMP_IR(NAME) NK_CAT(NklIrToken_, NAME),
#include "nkb/ir.inl"

#define X(TYPE, VALUE_TYPE) NK_CAT(NklIrToken_, TYPE),
    NKIR_NUMERIC_ITERATE(X)
#undef X

        NklIrToken_OperatorsBase,

    NklIrToken_LParen,
    NklIrToken_RParen,
    NklIrToken_LBrace,
    NklIrToken_RBrace,
    NklIrToken_Comma,
    NklIrToken_MinusGreater,

    NklIrToken_TagsBase,

    NklIrToken_AtTag,
};

#endif // NKL_CORE_IR_TOKENS_H_
