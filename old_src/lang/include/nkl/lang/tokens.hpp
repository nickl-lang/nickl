#ifndef HEADER_GUARD_NKL_LANG_TOKENS
#define HEADER_GUARD_NKL_LANG_TOKENS

namespace nkl {

enum ETokenID {
#define OP(ID, TEXT) t_##ID,
#define KW(ID) t_##ID,
#define SP(ID, TEXT) t_##ID,
#include "nkl/lang/tokens.inl"

    Token_Count,
};

extern const char *s_token_id[];
extern const char *s_token_text[];

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_TOKENS
