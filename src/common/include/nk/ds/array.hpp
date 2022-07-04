#ifndef HEADER_GUARD_NK_COMMON_ARRAY
#define HEADER_GUARD_NK_COMMON_ARRAY

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "nk/ds/private/container_base.hpp"
#include "nk/mem/mem.h"
#include "nk/utils/utils.hpp"

namespace nk {

template <class T>
struct Array
    : Slice<T>
    , ContainerBase<Array, T> {
    friend struct ContainerBase<Array, T>;

    using typename Slice<T>::value_type;

    using Slice<T>::data;
    using Slice<T>::size;
    size_t capacity;

    void deinit() {
        nk_platform_free(data);
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

    void _realloc(size_t n) {
        if (n > 0) {
            capacity = ceilToPowerOf2(size + n);
            T *new_data = (T *)nk_platform_alloc(capacity * sizeof(T));
            assert(new_data && "allocation failed");
            Slice<T>::copy({new_data, size});
            nk_platform_free(data);
            data = new_data;
        }
    }

    void _expand(size_t n) {
        size += n;
    }

    void _shrink(size_t n) {
        size -= n;
    }
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_ARRAY
