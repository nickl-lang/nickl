#ifndef HEADER_GUARD_NKL_CORE_TOKEN
#define HEADER_GUARD_NKL_CORE_TOKEN

#include <cstddef>
#include <cstdint>

#include "nk/common/string.hpp"

namespace nkl {

enum ETokenID {
#define OP(ID, TEXT) t_##ID,
#define KW(ID) t_##ID,
#define SP(ID, TEXT) t_##ID,
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

    uint8_t id = t_empty;
};

using TokenArray = Slice<Token>;

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_TOKEN
