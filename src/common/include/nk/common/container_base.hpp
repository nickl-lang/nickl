#ifndef HEADER_GUARD_NK_COMMON_CONTAINER_BASE
#define HEADER_GUARD_NK_COMMON_CONTAINER_BASE

#include <cassert>
#include <cstddef>

#include "nk/common/slice.hpp"

template <template <class> class TContainer, class T>
struct ContainerBase {
    void reserve(size_t n) {
        if (!self().enoughSpace(n)) {
            self()._realloc(n);
        }
    }

    Slice<T> push(size_t n = 1) {
        reserve(n);
        self()._expand(n);
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
    TContainer<T> &self() {
        return *(TContainer<T> *)this;
    }
};

#endif // HEADER_GUARD_NK_COMMON_CONTAINER_BASE
