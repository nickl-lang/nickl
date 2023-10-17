#ifndef HEADER_GUARD_NTK_STRING
#define HEADER_GUARD_NTK_STRING

#include <stddef.h>
#include <string.h>

#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/common.h"
#include "ntk/stream.h"

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

NK_INLINE nks nk_strcpy(NkAllocator alloc, nks src) {
    char *mem = (char *)nk_alloc(alloc, src.size);
    memcpy(mem, src.data, src.size);
    return LITERAL(nks){mem, src.size};
}

NK_INLINE nks nk_strcpy_nt(NkAllocator alloc, nks src) {
    char *mem = (char *)nk_alloc(alloc, src.size + 1);
    memcpy(mem, src.data, src.size);
    mem[src.size] = '\0';
    return LITERAL(nks){(char const *)mem, src.size};
}

NK_INLINE nks nks_trim_left(nks str) {
    while (str.size && nks_first(str) == ' ') {
        str.size -= 1;
        str.data += 1;
    }
    return str;
}

NK_INLINE nks nks_trim_right(nks str) {
    while (str.size && nks_last(str) == ' ') {
        str.size -= 1;
    }
    return str;
}

NK_INLINE nks nks_trim(nks str) {
    return nks_trim_right(nks_trim_left(str));
}

NK_INLINE nks nks_chop_by_delim(nks *str, char delim) {
    size_t i = 0;
    while (i < str->size && str->data[i] != delim) {
        i++;
    }

    nks res = {str->data, i};

    if (i < str->size) {
        str->data += i + 1;
        str->size -= i + 1;
    } else {
        str->data += i;
        str->size -= i;
    }

    return res;
}

NK_INLINE nks nks_chop_by_delim_reverse(nks *str, char delim) {
    nks res = *str;

    while (str->size && nks_last(*str) != delim) {
        str->size--;
    }

    if (str->size) {
        str->size--;

        res.data += str->size + 1;
        res.size -= str->size + 1;
    } else {
        res.data += str->size;
        res.size -= str->size;
    }

    return res;
}

NK_INLINE bool nks_equal(nks lhs, nks rhs) {
    return lhs.size == rhs.size && memcmp(lhs.data, rhs.data, lhs.size) == 0;
}

NK_INLINE bool nks_starts_with(nks str, nks pref) {
    if (pref.size > str.size) {
        return false;
    } else {
        return nks_equal(LITERAL(nks){str.data, pref.size}, pref);
    }
}

int nks_escape(nk_stream out, nks str);
int nks_unescape(nk_stream out, nks str);

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
    size_t operator()(::nks slice) {
        return ::hash_array((uint8_t *)&slice.data[0], (uint8_t *)&slice.data[slice.size]);
    }
};

} // namespace std

#endif // __cplusplus

#endif // HEADER_GUARD_NTK_STRING
