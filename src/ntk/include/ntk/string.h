#ifndef HEADER_GUARD_NTK_STRING
#define HEADER_GUARD_NTK_STRING

#include <stddef.h>
#include <string.h>

#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/common.h"
#include "ntk/stream.h"
#include "ntk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

nkav_typedef(char const, nks);

#define nks_begin nkav_begin
#define nks_end nkav_end

#define nks_first nkav_first
#define nks_last nkav_last

NK_INLINE nks nk_cs2s(char const *str) {
    return LITERAL(nks){str, strlen(str)};
}

nks nk_strcpy(NkAllocator alloc, nks src);
nks nk_strcpy_nt(NkAllocator alloc, nks src);

nks nks_trim_left(nks str);
nks nks_trim_right(nks str);
nks nks_trim(nks str);

nks nks_chop_by_delim(nks *str, char delim);
nks nks_chop_by_delim_reverse(nks *str, char delim);

hash_t nks_hash(nks str);
bool nks_equal(nks lhs, nks rhs);

bool nks_starts_with(nks str, nks pref);

int nks_escape(nk_stream out, nks str);
int nks_unescape(nk_stream out, nks str);
int nks_sanitize(nk_stream out, nks str);

#define nks_Fmt "%.*s"
#define nks_Arg(str) (int)(str).size, (str).data

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>

inline std::string_view std_view(nks str) {
    return std::string_view{str.data, str.size};
}

inline std::string std_str(nks str) {
    return std::string{std_view(str)};
}

inline std::ostream &operator<<(std::ostream &stream, nks str) {
    return stream << std::string_view{str.data, str.size};
}

inline bool operator==(nks lhs, nks rhs) {
    return nks_equal(lhs, rhs);
}

inline bool operator==(nks lhs, char const *rhs) {
    return lhs == nk_cs2s(rhs);
}

inline bool operator==(char const *lhs, nks rhs) {
    return nk_cs2s(lhs) == rhs;
}

namespace std {

template <>
struct hash<::nks> {
    size_t operator()(::nks str) {
        return nks_hash(str);
    }
};

} // namespace std

#endif // __cplusplus

#endif // HEADER_GUARD_NTK_STRING
