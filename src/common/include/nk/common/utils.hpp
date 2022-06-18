#ifndef HEADER_GUARD_NK_COMMON_UTILS_HPP
#define HEADER_GUARD_NK_COMMON_UTILS_HPP

#include <utility>

#include "nk/common/slice.hpp"
#include "nk/common/string.hpp"
#include "nk/common/utils.h"

#ifndef CAT
#define _CAT(x, y) x##y
#define CAT(x, y) _CAT(x, y)
#endif // CAT

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

inline hash_t hash_str(string str) {
    return hash_cstrn(str.data, str.size);
}

namespace std {

template <class T>
struct hash<::Slice<T>> {
    size_t operator()(::Slice<T> slice) {
        return ::hash_array((uint8_t *)&slice[0], (uint8_t *)&slice[slice.size]);
    }
};

template <class T>
struct equal_to<::Slice<T>> {
    size_t operator()(::Slice<T> lhs, ::Slice<T> rhs) {
        return lhs.size == rhs.size && memcmp(lhs.data, rhs.data, lhs.size * sizeof(T)) == 0;
    }
};

} // namespace std

#endif // HEADER_GUARD_NK_COMMON_UTILS_HPP
