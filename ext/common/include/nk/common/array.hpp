#ifndef HEADER_GUARD_ARRAY
#define HEADER_GUARD_ARRAY

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "nk/common/logger.hpp"
#include "nk/common/mem.hpp"
#include "nk/common/utils.hpp"

template <class T>
struct Array {
    size_t size;
    size_t capacity;
    T *data;

    static Array create(size_t cap = c_init_capacity) {
        Array ar;
        ar.init(cap);
        return ar;
    }

    void init(size_t cap = c_init_capacity) {
        assert(cap > 0 && "array capacity cannot be zero");

        size = 0;
        capacity = ceilToPowerOf2(cap);
        data = nullptr;

        _realloc(capacity);
    }

    void deinit() {
        lang_free(data);

        size = 0;
        capacity = ceilToPowerOf2(0);
        data = nullptr;
    }

    T &push(size_t n = 1) {
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
        assert(n <= size && "trying to pop more bytes that available");

        size -= n;

        T *res = _top();

        LOG_USE_SCOPE(arr)
        LOG_TRC("pop(size=%lu) -> %p", n * sizeof(T), res);

        return *res;
    }

    bool enoughSpace(size_t n) {
        return size + n <= capacity;
    }

    void clear() {
        size = 0;
    }

private:
    static constexpr size_t c_init_capacity = 1;

    T *_top() {
        assert(data && "uninitialized array access");
        return data + size;
    }

    void _realloc(size_t cap) {
        void *new_data = lang_realloc((void *)data, (capacity = ceilToPowerOf2(cap)) * sizeof(T));
        assert(new_data && "allocation failed");
        data = (T *)new_data;
    }
};

#endif // HEADER_GUARD_ARRAY
