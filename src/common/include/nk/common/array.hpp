#ifndef HEADER_GUARD_NK_COMMON_ARRAY
#define HEADER_GUARD_NK_COMMON_ARRAY

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "nk/common/logger.h"
#include "nk/common/mem.hpp"
#include "nk/common/profiler.hpp"
#include "nk/common/slice.hpp"
#include "nk/common/utils.hpp"

template <class T>
struct Array : Slice<T> {
    using Slice<T>::data;
    using Slice<T>::size;
    size_t capacity;

    static Array create(size_t cap = 0) {
        Array ar;
        ar.init(cap);
        return ar;
    }

    void init(size_t cap = 0) {
        size = 0;
        capacity = cap;
        data = nullptr;

        _realloc(capacity);
    }

    void deinit() {
        _mctx.def_allocator->free(data);

        size = 0;
        capacity = 0;
        data = nullptr;
    }

    T &push(size_t n = 1) {
        EASY_BLOCK("Array::push", profiler::colors::Grey200)

        if (!enoughSpace(n)) {
            _realloc(size + n);
        }

        size += n;
        T *res = _top() - n;

        LOG_USE_SCOPE(arr)
        LOG_TRC("push(size=%lu) -> %p", n * sizeof(T), res)

        return *res;
    }

    T &pop(size_t n = 1) {
        EASY_BLOCK("Array::pop", profiler::colors::Grey200)

        assert(n <= size && "trying to pop more bytes that available");

        size -= n;

        T *res = _top();

        LOG_USE_SCOPE(arr)
        LOG_TRC("pop(size=%lu) -> %p", n * sizeof(T), res);

        return *res;
    }

    bool enoughSpace(size_t n) const {
        return size + n <= capacity;
    }

    void clear() {
        size = 0;
    }

    Slice<T> slice(size_t i = 0, size_t n = -1ul) const {
        return {data + i, minu(n, size)};
    }

    void append(Slice<T const> slice) {
        slice.copy({&push(slice.size), slice.size});
    }

private:
    T *_top() const {
        assert((!size || data) && "uninitialized array access");
        return data + size;
    }

    void _realloc(size_t cap) {
        if (cap > 0) {
            capacity = ceilToPowerOf2(cap);
            void *new_data = _mctx.def_allocator->alloc(capacity * sizeof(T));
            assert(new_data && "allocation failed");
            slice().copy({(T *)new_data, size});
            _mctx.def_allocator->free(data);
            data = (T *)new_data;
        }
    }
};

#endif // HEADER_GUARD_NK_COMMON_ARRAY
