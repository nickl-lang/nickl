#ifndef HEADER_GUARD_NTK_LOG_ARRAY
#define HEADER_GUARD_NTK_LOG_ARRAY

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "ntk/allocator.h"
#include "ntk/utils.h"

template <class T>
struct NkLogArray {
    using value_type = T;

    using _block_ptr = T *;

    usize _size{};
    _block_ptr *_block_table{};
    usize _block_table_size{};
    usize _block_table_capacity{};
    usize _bi{}; // block index
    usize _ic{}; // initial capacity
    NkAllocator _alloc{nk_default_allocator};

    void deinit() {
        for (usize bi = 0; bi < _block_table_size; bi++) {
            nk_free_t(_alloc, _block_table[bi], _blockDataSize(bi));
        }

        nk_free_t(_alloc, _block_table, _block_table_capacity);
    }

    usize size() const {
        return _size;
    }

    T &at(usize i) const {
        return *(_blockByIndex(i) + _indexRemainder(i));
    }

    T &operator[](usize pos) const {
        return at(pos);
    }

    void reserve(usize n) {
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

            for (usize bi = _block_table_size; bi <= _bi; bi++) {
                _allocateBlock(bi);
            }
        }
    }

    T *push(usize n = 1) {
        reserve(n);
        _size += n;
        return _top() - n;
    }

    void pop(usize n = 1) {
        assert(n <= _size && "trying to pop more bytes that available");
        _size -= n;
        _bi = _size == 0 ? 0 : _blockIndexByIndex(_size - 1);
    }

    void clear() {
        pop(_size);
    }

private:
    static constexpr usize INIT_BLOCK_TAB_CAPACITY = 8;

    usize _indexRemainder(usize i) const {
        return i + _ic - floorToPowerOf2(i + _ic);
    }

    usize _precedingBlocksSize(usize bi) const {
        return ((1ul << bi) - 1) * _ic;
    }

    usize _blockIndexByIndex(usize i) const {
        return log2u64(i / _ic + 1);
    }

    _block_ptr _blockByIndex(usize i) const {
        return _block_table[_blockIndexByIndex(i)];
    }

    usize _blockDataSize(usize bi) const {
        return _ic * (1ul << bi);
    }

    void _allocateBlock(usize bi) {
        if (_block_table_size == _block_table_capacity) {
            _block_table_capacity <<= 1;
            _block_table = nk_realloc_t(_alloc, _block_table_capacity, _block_table, _block_table_capacity >> 1);
        }
        _block_table[_block_table_size++] = nk_alloc_t<T>(_alloc, _blockDataSize(bi));
    }

    T *_top() const {
        return _block_table ? (_size == 0 ? &at(0) : &at(_size - 1) + 1) : 0;
    }

    bool _enoughSpace(usize n) const {
        i64 const rem = _size == 0 ? -1 : (i64)_indexRemainder(_size - 1);
        return rem + n < _blockDataSize(_bi);
    }
};

#endif // HEADER_GUARD_NTK_LOG_ARRAY
