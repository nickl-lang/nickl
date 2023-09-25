#ifndef HEADER_GUARD_NK_COMMON_ARRAY
#define HEADER_GUARD_NK_COMMON_ARRAY

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "nk/common/allocator.hpp"
#include "nk/common/slice.hpp"
#include "nk/common/utils.hpp"

template <class T>
struct NkArray : NkSlice<T> {
    using NkSlice<T>::_data;
    using NkSlice<T>::_size;
    size_t _capacity;
    NkAllocator _alloc;

    static NkArray create() {
        return create({});
    }

    static NkArray create(NkAllocator alloc) {
        NkArray ar{};
        ar._alloc = alloc;
        return ar;
    }

    size_t capacity() const {
        return _capacity;
    }

    void deinit() {
        auto const alloc = _alloc.proc ? _alloc : nk_default_allocator;
        nk_free_t(alloc, _data, _capacity);

        _data = {};
        _size = {};
        _capacity = {};
    }

    bool reserve(size_t n) {
        if (_size + n > _capacity) {
            auto const new_capacity = ceilToPowerOf2(_size + n);
            auto const alloc = _alloc.proc ? _alloc : nk_default_allocator;
            auto const new_data = nk_realloc_t(alloc, new_capacity, _data, _capacity);
            if (!new_data) {
                return false;
            }
            _data = new_data;
            _capacity = new_capacity;
        }
        return true;
    }

    NkSlice<T> push(size_t n = 1) {
        if (reserve(n)) {
            _size += n;
            return {_data + _size - n, n};
        } else {
            return {};
        }
    }

    template <class... TArgs>
    T &emplace(TArgs &&... args) {
        return *new (push().data()) T{std::forward<TArgs>(args)...};
    }

    void pop(size_t n = 1) {
        assert(n <= _size && "trying to pop more bytes that available");
        _size -= n;
    }

    void clear() {
        pop(_size);
    }

    void append(NkSlice<T const> slice) {
        std::memcpy(&*push(slice.size()), slice.data(), slice.size() * sizeof(T));
    }
};

#endif // HEADER_GUARD_NK_COMMON_ARRAY
