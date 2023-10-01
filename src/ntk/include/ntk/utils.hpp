#ifndef HEADER_GUARD_NTK_UTILS_HPP
#define HEADER_GUARD_NTK_UTILS_HPP

#include <cstdarg>
#include <string>
#include <utility>

#include "ntk/utils.h"

#ifndef defer
struct _DeferDummy {};
template <class F>
struct _Deferrer {
    F f;
    ~_Deferrer() {
        f();
    }
};
template <class F>
_Deferrer<F> operator*(_DeferDummy, F &&f) {
    return {std::forward<F>(f)};
}
#define defer auto CAT(__defer, __LINE__) = _DeferDummy{} *[&]()
#endif // defer

template <class F>
[[nodiscard]] _Deferrer<F> makeDeferrer(F &&f) {
    return {std::forward<F>(f)};
}

template <class T, class F>
struct _DeferrerWithData {
    T data;
    F f;
    operator T() const {
        return data;
    }
    ~_DeferrerWithData() {
        f();
    }
};

template <class T, class F>
[[nodiscard]] _DeferrerWithData<T, F> makeDeferrerWithData(T &&data, F &&f) {
    return {std::forward<T>(data), std::forward<F>(f)};
}

#endif // HEADER_GUARD_NTK_UTILS_HPP
