#ifndef HEADER_GUARD_NK_COMMON_CONTAINER_BASE
#define HEADER_GUARD_NK_COMMON_CONTAINER_BASE

#include <cassert>
#include <cstddef>

#include "nk/common/slice.hpp"

template <template <class> class TContainer, class T>
struct ContainerBase {
    using value_type = T;

    void reserve(size_t n) {
        if (!self().enoughSpace(n)) {
            self()._realloc(n);
        }
    }

    Slice<T> push(size_t n = 1) {
        return push_aligned(0, n);
    }

    Slice<T> push_aligned(size_t align, size_t n = 1) {
        reserve(n + align * alignof(T));
        self()._expand(align ? _align(align, n) : n);
        return {self()._top() - n, n};
    }

    void pop(size_t n = 1) {
        assert(n <= self().size && "trying to pop more bytes that available");
        self()._shrink(n);
    }

    void clear() {
        pop(self().size);
    }

    void append(Slice<T const> slice) {
        slice.copy(push(slice.size));
    }

private:
    size_t _align(size_t align, size_t n) {
        size_t const top = (size_t)self()._top();
        size_t const padding = roundUp(top, align * alignof(T)) - top;
        return n + roundUp(padding, sizeof(T)) / sizeof(T);
    }

    TContainer<T> &self() {
        return *(TContainer<T> *)this;
    }
};

#endif // HEADER_GUARD_NK_COMMON_CONTAINER_BASE
