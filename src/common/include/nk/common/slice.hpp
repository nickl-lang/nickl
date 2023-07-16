#ifndef HEADER_GUARD_NK_COMMON_SLICE
#define HEADER_GUARD_NK_COMMON_SLICE

#include <cstddef>
#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <type_traits>

#include "nk/common/utils.hpp"

template <class T>
struct NkSlice {
    using value_type = T;
    using pointer = value_type *;
    using iterator = pointer;
    using reference = value_type &;
    using size_type = size_t;

    pointer _data;
    size_type _size;

    constexpr iterator data() const {
        return _data;
    }

    constexpr size_type size() const {
        return _size;
    }

    constexpr reference at(size_type pos) const {
        return _data[pos];
    }

    constexpr reference operator[](size_type pos) const {
        return _data[pos];
    }

    constexpr reference front() const {
        return _data[0];
    }

    constexpr reference back() const {
        return _data[_size - 1];
    }

    constexpr reference operator*() const {
        return _data[0];
    }

    constexpr iterator begin() const noexcept {
        return _data;
    }

    constexpr iterator end() const noexcept {
        return _data + _size;
    }

    constexpr operator NkSlice<T const>() const {
        return {_data, _size};
    }
};

namespace std {

template <class T>
struct hash<::NkSlice<T>> {
    size_t operator()(::NkSlice<T> slice) {
        static_assert(is_trivial_v<T>, "T should be trivial");
        return ::hash_array((uint8_t *)&slice[0], (uint8_t *)&slice[slice.size()]);
    }
};

template <class T>
struct equal_to<::NkSlice<T>> {
    size_t operator()(::NkSlice<T> lhs, ::NkSlice<T> rhs) {
        static_assert(is_trivial_v<T>, "T should be trivial");
        return lhs.size() == rhs.size() && memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
    }
};

} // namespace std

#endif // HEADER_GUARD_NK_COMMON_SLICE
