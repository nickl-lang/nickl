#ifndef NTK_STRING_H_
#define NTK_STRING_H_

#include <string.h>

#include "ntk/allocator.h"
#include "ntk/common.h"
#include "ntk/slice.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef NkSlice(char const) NkString;

#define NKS_INIT NK_SLICE_INIT

#define nks_begin nk_slice_begin
#define nks_end nk_slice_end

#define nks_first nk_slice_first
#define nks_last nk_slice_last

NK_INLINE NkString nk_cs2s(char const *str) {
    return NK_LITERAL(NkString){str, strlen(str)};
}

NkString nks_copy(NkAllocator alloc, NkString src);
NkString nks_copyNt(NkAllocator alloc, NkString src);

NkString nks_trimLeft(NkString str);
NkString nks_trimRight(NkString str);
NkString nks_trim(NkString str);

NkString nks_chopByDelim(NkString *str, char delim);
NkString nks_chopByDelimReverse(NkString *str, char delim);

u64 nks_hash(NkString str);
bool nks_equal(NkString lhs, NkString rhs);

bool nks_startsWith(NkString str, NkString pref);

i32 nks_escape(NkStream out, NkString str);
i32 nks_unescape(NkStream out, NkString str);
i32 nks_sanitize(NkStream out, NkString str);

#define NKS_FMT "%.*s"
#define NKS_ARG(str) (i32)(str).size, (str).data

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>

inline std::string_view nk_s2stdView(NkString str) {
    return std::string_view{str.data, str.size};
}

inline std::string nk_s2stdStr(NkString str) {
    return std::string{nk_s2stdView(str)};
}

inline std::ostream &operator<<(std::ostream &stream, NkString str) {
    return stream << std::string_view{str.data, str.size};
}

inline bool operator==(NkString lhs, NkString rhs) {
    return nks_equal(lhs, rhs);
}

inline bool operator==(NkString lhs, char const *rhs) {
    return lhs == nk_cs2s(rhs);
}

inline bool operator==(char const *lhs, NkString rhs) {
    return nk_cs2s(lhs) == rhs;
}

namespace std {

template <>
struct hash<::NkString> {
    usize operator()(::NkString str) {
        return nks_hash(str);
    }
};

} // namespace std

#endif // __cplusplus

#endif // NTK_STRING_H_
