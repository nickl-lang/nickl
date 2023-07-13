#ifndef HEADER_GUARD_NK_COMMON_ARRAY
#define HEADER_GUARD_NK_COMMON_ARRAY

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "nk/common/allocator.h"
#include "nk/common/slice.hpp"
#include "nk/common/utils.hpp"

template <class T>
struct nkarray : nkslice<T> {
    using nkslice<T>::_data;
    using nkslice<T>::_size;
    size_t _capacity{};
    NkAllocator _alloc{nk_default_allocator};

    size_t capacity() const {
        return _capacity;
    }

    void deinit() {
        nk_free(_alloc, _data);
        *this = {};
    }

    void reserve(size_t n) {
        if (_size + n > _capacity) {
            auto const new_capacity = ceilToPowerOf2(_size + n);
            auto const new_data = (T *)nk_realloc(_alloc, new_capacity * sizeof(T), _data, _capacity * sizeof(T));
            assert(new_data && "allocation failed");
            _data = new_data;
            _capacity = new_capacity;
        }
    }

    nkslice<T> push(size_t n = 1) {
        reserve(n);
        _size += n;
        return {_data + _size - n, n};
    }

    void pop(size_t n = 1) {
        assert(n <= _size && "trying to pop more bytes that available");
        _size -= n;
    }

    void clear() {
        pop(_size);
    }

    void append(nkslice<T const> slice) {
        std::memcpy(&*push(slice.size()), slice.data(), slice.size() * sizeof(T));
    }
};

#endif // HEADER_GUARD_NK_COMMON_ARRAY
