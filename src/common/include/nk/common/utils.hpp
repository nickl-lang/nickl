#ifndef HEADER_GUARD_NK_COMMON_UTILS_HPP
#define HEADER_GUARD_NK_COMMON_UTILS_HPP

#include <utility>

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

#endif // HEADER_GUARD_NK_COMMON_UTILS_HPP
