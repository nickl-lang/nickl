#ifndef HEADER_GUARD_NK_COMMON_ARRAY
#define HEADER_GUARD_NK_COMMON_ARRAY

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "nk/common/allocator.hpp"
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
        nk_free_t(_alloc, _data, _capacity);
        *this = {};
    }

    bool reserve(size_t n) {
        if (_size + n > _capacity) {
            auto const new_capacity = ceilToPowerOf2(_size + n);
            auto const new_data = nk_realloc_t(_alloc, new_capacity, _data, _capacity);
            if (!new_data) {
                return false;
            }
            _data = new_data;
            _capacity = new_capacity;
        }
        return true;
    }

    nkslice<T> push(size_t n = 1) {
        if (reserve(n)) {
            _size += n;
            return {_data + _size - n, n};
        } else {
            return {};
        }
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
