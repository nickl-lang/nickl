#ifndef HEADER_GUARD_NK_COMMON_SLICE
#define HEADER_GUARD_NK_COMMON_SLICE

#include <cstddef>
#include <cstring>
#include <type_traits>

#include "nk/common/mem.hpp"

template <class T>
struct Slice {
    T *data;
    size_t size;

    using value_type = T;
    using iterator = T *;
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
};

template <class TSlice>
TSlice copy(TSlice slice, Allocator &allocator) {
    using T = std::decay_t<typename TSlice::value_type>;
    T *mem = allocator.alloc<T>(slice.size);
    std::memcpy(mem, slice.data, slice.size * sizeof(T));
    return {mem, slice.size};
}

template <class TSlice, class T = std::decay_t<typename TSlice::value_type>>
T *copy(TSlice slice, T *buf) {
    std::memcpy(buf, slice.data, slice.size * sizeof(T));
    return buf + slice.size;
}

#endif // HEADER_GUARD_NK_COMMON_SLICE
