#ifndef HEADER_GUARD_NK_COMMON_ARRAY
#define HEADER_GUARD_NK_COMMON_ARRAY

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "nk/common/logger.hpp"
#include "nk/common/mem.hpp"
#include "nk/common/profiler.hpp"
#include "nk/common/slice.hpp"
#include "nk/common/utils.hpp"

template <class T>
struct Array {
    T *data;
    size_t size;
    size_t capacity;

    static Array create(size_t cap = c_init_capacity) {
        Array ar;
        ar.init(cap);
        return ar;
    }

    void init(size_t cap = c_init_capacity) {
        size = 0;
        capacity = cap;
        data = nullptr;

        _realloc(capacity);
    }

    void deinit() {
        lang_free(data);

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
        return {data + i, std::min(n, size)};
    }

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

private:
    static constexpr size_t c_init_capacity = 0;

    T *_top() const {
        assert(data && "uninitialized array access");
        return data + size;
    }

    void _realloc(size_t cap) {
        if (cap > 0) {
            void *new_data =
                lang_realloc((void *)data, (capacity = ceilToPowerOf2(cap)) * sizeof(T));
            assert(new_data && "allocation failed");
            data = (T *)new_data;
        }
    }
};

#endif // HEADER_GUARD_NK_COMMON_ARRAY
