#ifndef HEADER_GUARD_NK_COMMON_UTILS_HPP
#define HEADER_GUARD_NK_COMMON_UTILS_HPP

#include <utility>

#include "nk/common/utils.h"

#ifndef defer
struct _DeferDummy {};
template <class F>
struct _Deferrer {
    F f;
    ~_Deferrer() {
        f();
    }
    _Deferrer(F &&f)
        : f{std::move(f)} {
    }
    _Deferrer(_Deferrer<F> const &) = delete;
};
template <class F>
_Deferrer<F> operator*(_DeferDummy, F &&f) {
    return {std::forward<F>(f)};
}
#define defer auto CAT(__defer, __LINE__) = _DeferDummy{} *[&]()
#endif // defer

template <class F>
[[nodiscard]] _Deferrer<F> createDeferrer(F &&f) {
    return {std::forward<F>(f)};
}

#endif // HEADER_GUARD_NK_COMMON_UTILS_HPP
