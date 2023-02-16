#ifndef HEADER_GUARD_NKL_LANG_TOKEN_HPP
#define HEADER_GUARD_NKL_LANG_TOKEN_HPP

enum ETokenId {
#define OP(ID, TEXT) t_##ID,
#define KW(ID) t_##ID,
#define SP(ID, TEXT) t_##ID,
#include "tokens.inl"

    Token_Count,
};

extern const char *s_token_id[];
extern const char *s_token_text[];

#endif // HEADER_GUARD_NKL_LANG_TOKEN_HPP
