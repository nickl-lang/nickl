#ifndef HEADER_GUARD_NK_COMMON_ARRAY
#define HEADER_GUARD_NK_COMMON_ARRAY

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "nk/common/container_base.hpp"
#include "nk/common/mem.h"
#include "nk/common/utils.hpp"

template <class T>
struct Array
    : Slice<T>
    , ContainerBase<Array, T> {
    friend struct ContainerBase<Array, T>;

    using Slice<T>::data;
    using Slice<T>::size;
    size_t capacity;

    void reserve(size_t cap) {
        _expand(cap);
    }

    void deinit() {
        platform_free(data);
        *this = {};
    }

    bool enoughSpace(size_t n) const {
        return size + n <= capacity;
    }

private:
    T *_top() const {
        assert((!size || data) && "uninitialized array access");
        return data + size;
    }

    void _expand(size_t n) {
        if (n > 0) {
            capacity = ceilToPowerOf2(size + n);
            void *new_data = platform_alloc(capacity * sizeof(T));
            assert(new_data && "allocation failed");
            Slice<T>::copy({(T *)new_data, size});
            platform_free(data);
            data = (T *)new_data;
        }
    }

    void _shrink(size_t /*n*/) {
    }
};

#endif // HEADER_GUARD_NK_COMMON_ARRAY
