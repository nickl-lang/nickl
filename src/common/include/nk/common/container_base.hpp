#ifndef HEADER_GUARD_NK_COMMON_CONTAINER_BASE
#define HEADER_GUARD_NK_COMMON_CONTAINER_BASE

#include <cassert>
#include <cstddef>

#include "nk/common/slice.hpp"

template <template <class> class TContainer, class T>
struct ContainerBase {
    Slice<T> push(size_t n = 1) {
        if (!self().enoughSpace(n)) {
            self()._expand(n);
        }
        self().size += n;
        return {self()._top() - n, n};
    }

    Slice<T> pop(size_t n = 1) {
        assert(n <= self().size && "trying to pop more bytes that available");
        self().size -= n;
        self()._shrink(n);
        return {self()._top(), n};
    }

    void clear() {
        self().pop(self().size);
    }

    void append(Slice<T const> slice) {
        slice.copy(self().push(slice.size));
    }

private:
    TContainer<T> &self() {
        return *(TContainer<T> *)this;
    }
};

#endif // HEADER_GUARD_NK_COMMON_CONTAINER_BASE
