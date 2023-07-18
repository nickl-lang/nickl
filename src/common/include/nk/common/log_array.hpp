#ifndef HEADER_GUARD_NK_COMMON_LOG_ARRAY
#define HEADER_GUARD_NK_COMMON_LOG_ARRAY

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "nk/common/allocator.hpp"
#include "nk/common/array.hpp"
#include "nk/common/slice.hpp"

template <class T>
struct NkLogArray {
    using value_type = T;

    using _block_ptr = T *;

    size_t _size{};
    _block_ptr *_block_table{};
    size_t _block_table_size{};
    size_t _block_table_capacity{};
    size_t _bi{}; // block index
    size_t _ic{}; // initial capacity
    NkAllocator _alloc{nk_default_allocator};

    void deinit() {
        for (size_t bi = 0; bi < _block_table_size; bi++) {
            nk_free_t(_alloc, _block_table[bi], _blockDataSize(bi));
        }

        nk_free_t(_alloc, _block_table, _block_table_capacity);
    }

    size_t size() const {
        return _size;
    }

    T &at(size_t i) const {
        return *(_blockByIndex(i) + _indexRemainder(i));
    }

    T &operator[](size_t pos) const {
        return at(pos);
    }

    void reserve(size_t n) {
        if (!_enoughSpace(n)) {
            if (!_block_table) {
                _block_table_capacity = INIT_BLOCK_TAB_CAPACITY;
                _block_table = nk_alloc_t<_block_ptr>(_alloc, _block_table_capacity);
                _ic = ceilToPowerOf2(n);
            }

            if (!_enoughSpace(n)) {
                auto const exp = ceilToPowerOf2(n);
                _bi = maxu(exp >= _ic ? log2u64(exp / _ic) : 0ull, _bi + 1);
            }
            _size = _precedingBlocksSize(_bi);

            for (size_t bi = _block_table_size; bi <= _bi; bi++) {
                _allocateBlock(bi);
            }
        }
    }

    NkSlice<T> push(size_t n = 1) {
        reserve(n);
        _size += n;
        return {_top() - n, n};
    }

    void pop(size_t n = 1) {
        assert(n <= _size && "trying to pop more bytes that available");
        _size -= n;
        _bi = _size == 0 ? 0 : _blockIndexByIndex(_size - 1);
    }

    void clear() {
        pop(_size);
    }

    void append(NkSlice<T const> slice) {
        std::memcpy(&*push(slice.size()), slice.data(), slice.size() * sizeof(T));
    }

private:
    static constexpr size_t INIT_BLOCK_TAB_CAPACITY = 8;

    size_t _indexRemainder(size_t i) const {
        return i + _ic - floorToPowerOf2(i + _ic);
    }

    size_t _precedingBlocksSize(size_t bi) const {
        return ((1ul << bi) - 1) * _ic;
    }

    size_t _blockIndexByIndex(size_t i) const {
        return log2u64(i / _ic + 1);
    }

    _block_ptr _blockByIndex(size_t i) const {
        return _block_table[_blockIndexByIndex(i)];
    }

    size_t _blockDataSize(size_t bi) const {
        return _ic * (1ul << bi);
    }

    void _allocateBlock(size_t bi) {
        if (_block_table_size == _block_table_capacity) {
            _block_table_capacity <<= 1;
            _block_table = nk_realloc_t(_alloc, _block_table_capacity, _block_table, _block_table_capacity >> 1);
        }
        _block_table[_block_table_size++] = nk_alloc_t<T>(_alloc, _blockDataSize(bi));
    }

    T *_top() const {
        return _block_table ? (_size == 0 ? &at(0) : &at(_size - 1) + 1) : 0;
    }

    bool _enoughSpace(size_t n) const {
        int64_t const rem = _size == 0 ? -1 : (int64_t)_indexRemainder(_size - 1);
        return rem + n < _blockDataSize(_bi);
    }
};

#endif // HEADER_GUARD_NK_COMMON_LOG_ARRAY
