#ifndef HEADER_GUARD_NK_COMMON_UTILS_HPP
#define HEADER_GUARD_NK_COMMON_UTILS_HPP

#include <cstdarg>
#include <string>
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

std::string string_vformat(char const *fmt, va_list ap);

#define nkslice_iterate(slice)                   \
    _nk_slice_iterate_wrapper<decltype(slice)> { \
        slice                                    \
    }

template <class TSlice>
struct _nk_slice_iterate_wrapper {
    TSlice _slice;
    using pointer = decltype(TSlice::data);
    pointer begin() {
        return _slice.data;
    }
    pointer end() {
        return _slice.data + _slice.size;
    }
};

#endif // HEADER_GUARD_NK_COMMON_UTILS_HPP
