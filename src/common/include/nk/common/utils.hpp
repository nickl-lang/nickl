#ifndef HEADER_GUARD_NK_COMMON_UTILS_HPP
#define HEADER_GUARD_NK_COMMON_UTILS_HPP

#include <type_traits>
#include <utility>

#include "nk/common/slice.hpp"
#include "nk/common/string.hpp"
#include "nk/common/string_builder.hpp"
#include "nk/common/utils.h"

#ifndef defer
struct nk_DeferDummy {};
template <class F>
struct nk_Deferrer {
    F f;
    ~nk_Deferrer() {
        f();
    }
};
template <class F>
nk_Deferrer<F> operator*(nk_DeferDummy, F &&f) {
    return {std::forward<F>(f)};
}
#define defer auto CAT(nk_defer, __LINE__) = nk_DeferDummy{} *[&]()
#endif // defer

namespace nk {

inline hash_t hash_str(string str) {
    return hash_cstrn(str.data, str.size);
}

void string_escape(StringBuilder &sb, string str);
void string_unescape(StringBuilder &sb, string str);

template <class THeader, class TElem>
size_t arrayWithHeaderSize(size_t n) {
    return roundUp(sizeof(THeader), alignof(TElem)) + sizeof(TElem) * n;
}

template <class THeader, class TElem>
TElem *arrayWithHeaderData(THeader *header) {
    return (TElem *)roundUp((size_t)(header + 1), alignof(TElem));
}

} // namespace nk

namespace std {

template <class T>
struct hash<::nk::Slice<T>> {
    size_t operator()(::nk::Slice<T> slice) {
        static_assert(is_trivial_v<T>, "T should be trivial");
        return ::hash_array((uint8_t *)&slice[0], (uint8_t *)&slice[slice.size]);
    }
};

template <class T>
struct equal_to<::nk::Slice<T>> {
    size_t operator()(::nk::Slice<T> lhs, ::nk::Slice<T> rhs) {
        static_assert(is_trivial_v<T>, "T should be trivial");
        return lhs.size == rhs.size && memcmp(lhs.data, rhs.data, lhs.size * sizeof(T)) == 0;
    }
};

} // namespace std

#endif // HEADER_GUARD_NK_COMMON_UTILS_HPP
