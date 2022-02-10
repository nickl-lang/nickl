#include "nkl/core/token.hpp"

namespace nkl {

const char *s_token_id[] = {
#define OP(id, text) #id,
#define KW(id) #id,
#define SP(id) #id,
#include "nkl/core/tokens.inl"
};

const char *s_token_text[] = {
#define OP(id, text) text,
#define KW(id) #id,
#define SP(id) "",
#include "nkl/core/tokens.inl"
};

} // namespace nkl
