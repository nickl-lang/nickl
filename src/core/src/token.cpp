#include "nkl/core/token.hpp"

namespace nkl {

const char *s_token_id[] = {
#define OP(ID, TEXT) #ID,
#define KW(ID) #ID,
#define SP(ID, TEXT) #ID,
#include "nkl/core/tokens.inl"
};

const char *s_token_text[] = {
#define OP(ID, TEXT) TEXT,
#define KW(ID) #ID,
#define SP(ID, TEXT) TEXT,
#include "nkl/core/tokens.inl"
};

} // namespace nkl
