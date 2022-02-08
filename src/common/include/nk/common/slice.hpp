#ifndef HEADER_GUARD_NK_COMMON_SLICE
#define HEADER_GUARD_NK_COMMON_SLICE

#include <cassert>
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

    void copy(Slice<T> &dst, Allocator &allocator) const {
        auto mem = allocator.alloc<std::decay_t<T>>(size);
        std::memcpy(mem, data, size * sizeof(T));
        dst = {mem, size};
    }

    void copy(Slice<std::decay_t<T>> dst) const {
        assert(dst.size >= size && "copying to a slice of insufficient size");
        std::memcpy(dst.data, data, size * sizeof(T));
    }
};

#endif // HEADER_GUARD_NK_COMMON_SLICE
