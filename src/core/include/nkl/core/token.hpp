#ifndef HEADER_GUARD_NKL_CORE_TOKEN
#define HEADER_GUARD_NKL_CORE_TOKEN

#include <cstddef>

#include "nk/common/string.hpp"

namespace nkl {

enum ETokenID {
#define OP(id, text) Token_##id,
#define KW(id) Token_##id,
#define SP(id) Token_##id,
#include "nkl/core/tokens.inl"

    Token_Count,
};

extern const char *s_token_id[];
extern const char *s_token_text[];

struct Token {
    string text;

    size_t pos;
    size_t lin;
    size_t col;

    ETokenID id = Token_empty;
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_TOKEN
