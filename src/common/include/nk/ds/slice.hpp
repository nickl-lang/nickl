#ifndef HEADER_GUARD_NK_COMMON_SLICE
#define HEADER_GUARD_NK_COMMON_SLICE

#include <cassert>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <type_traits>

#include "nk/utils/utils.h"

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
        for (size_t i = 0; i < size; i++) {
            dst[i] = data[i];
        }
        return {dst, size};
    }

    void fill(T const &value) {
        for (size_t i = 0; i < size; i++) {
            data[i] = value;
        }
    };

    Slice<T> slice(size_t i = 0, size_t n = -1ull) const {
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

    bool equals(Slice<std::decay_t<T> const> other) const {
        return size == other.size && std::memcmp(data, other.data, size) == 0;
    }
};

} // namespace nk

#define _SLICE(TYPE, NAME)                                                          \
    Slice<TYPE> NAME {                                                              \
        CAT(__buf_, NAME), sizeof(CAT(__buf_, NAME)) / sizeof(CAT(__buf_, NAME)[0]) \
    }

#define ARRAY_SLICE(TYPE, NAME, SIZE) \
    TYPE CAT(__buf_, NAME)[SIZE];     \
    _SLICE(TYPE, NAME)

#define ARRAY_SLICE_INIT(TYPE, NAME, ...)     \
    TYPE CAT(__buf_, NAME)[] = {__VA_ARGS__}; \
    _SLICE(TYPE, NAME)

#define SLICE_COPY_LSTRIDE(DST, FIELD, SRC)                                      \
    for (size_t _i = 0; _i < (SRC).size; _i++) {                                 \
        (&(DST)[0].FIELD)[_i * sizeof((DST)[0]) / sizeof((SRC)[0])] = (SRC)[_i]; \
    }

#define SLICE_COPY_RSTRIDE(DST, SRC, FIELD)                                      \
    for (size_t _i = 0; _i < (SRC).size; _i++) {                                 \
        (DST)[_i] = (&(SRC)[0].FIELD)[_i * sizeof((SRC)[0]) / sizeof((DST)[0])]; \
    }

#endif // HEADER_GUARD_NK_COMMON_SLICE
