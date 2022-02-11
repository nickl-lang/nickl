#ifndef HEADER_GUARD_NKL_CORE_TOKEN
#define HEADER_GUARD_NKL_CORE_TOKEN

#include <cstddef>

#include "nk/common/string.hpp"

namespace nkl {

enum ETokenID {
#define OP(id, text) t_##id,
#define KW(id) t_##id,
#define SP(id) t_##id,
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

    ETokenID id = t_empty;
};

using TokenArray = Slice<Token>;

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_TOKEN
