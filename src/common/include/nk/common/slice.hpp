#ifndef HEADER_GUARD_NK_COMMON_SLICE
#define HEADER_GUARD_NK_COMMON_SLICE

#include <cassert>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <type_traits>

#include "nk/common/utils.h"

namespace nk {

template <class T>
struct Slice {
    T *data;
    size_t size;

    using value_type = T;
    using iterator = T *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using reference = value_type &;
    using size_type = size_t;

    constexpr reference at(size_type pos) const {
        return data[pos];
    }

    constexpr reference operator[](size_type pos) const {
        return data[pos];
    }

    constexpr reference front() const {
        return data[0];
    }

    constexpr reference back() const {
        return data[size - 1];
    }

    constexpr iterator begin() const noexcept {
        return data;
    }

    constexpr iterator end() const noexcept {
        return data + size;
    }

    constexpr reverse_iterator rbegin() const noexcept {
        return reverse_iterator{end()};
    }

    constexpr reverse_iterator rend() const noexcept {
        return reverse_iterator{begin()};
    }

    Slice<std::decay_t<T>> copy(Slice<std::decay_t<T>> dst) const {
        assert(dst.size >= size && "copying to a slice of insufficient size");
        return copy(dst.data);
    }

    Slice<std::decay_t<T>> copy(std::decay_t<T> *dst) const {
        std::memcpy(dst, data, size * sizeof(T));
        return {dst, size};
    }

    Slice<T> slice(size_t i = 0, size_t n = -1ul) const {
        assert(i <= size && "index out of range");
        return {data + i, minu(n, size - i)};
    }

    operator Slice<T const>() const {
        return {data, size};
    }

    operator T *() const {
        return data;
    }

    T &operator*() const {
        return *data;
    }

    T *operator->() const {
        return data;
    }
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_SLICE
