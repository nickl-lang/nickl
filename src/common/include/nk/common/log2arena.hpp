#ifndef HEADER_GUARD_NK_COMMON_LOG2_ARENA
#define HEADER_GUARD_NK_COMMON_LOG2_ARENA

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "nk/common/array.hpp"
#include "nk/common/private/container_base.hpp"

template <class T>
struct Log2Arena : ContainerBase<Log2Arena, T> {
    friend struct ContainerBase<Log2Arena, T>;

    using _block_ptr = T *;

    size_t size;

    Array<_block_ptr> _block_table;
    size_t _bi; // block index
    size_t _ic; // initial capacity

    void deinit() {
        _block_ptr *pblock = _block_table.data;
        for (_block_ptr *end = pblock + _block_table.size; pblock != end; pblock++) {
            platform_free(*pblock);
        }

        _block_table.deinit();
    }

    T &at(size_t i) const {
        return *(_blockByIndex(i) + _indexRemainder(i));
    }

    T &operator[](size_t pos) const {
        return at(pos);
    }

    bool enoughSpace(size_t n) const {
        int64_t const rem = size == 0 ? -1 : (int64_t)_indexRemainder(size - 1);
        return rem + n < _blockDataSize(_bi);
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
        return log2u(i / _ic + 1);
    }

    _block_ptr _blockByIndex(size_t i) const {
        return _block_table[_blockIndexByIndex(i)];
    }

    size_t _blockDataSize(size_t bi) const {
        return _ic * (1ul << bi);
    }

    void _allocateBlock(size_t bi) {
        *_block_table.push() = (_block_ptr)platform_alloc(_blockDataSize(bi) * sizeof(T));
    }

    T *_top() const {
        return _block_table.data ? (size == 0 ? &at(0) : &at(size - 1) + 1) : 0;
    }

    void _realloc(size_t n) {
        if (!_block_table.data) {
            _block_table.reserve(INIT_BLOCK_TAB_CAPACITY);
            _ic = ceilToPowerOf2(n);
        }

        if (!enoughSpace(n)) {
            _bi = maxu(log2u(ceilToPowerOf2(n) / _ic), _bi + 1);
        }
        size = _precedingBlocksSize(_bi);

        for (size_t bi = _block_table.size; bi <= _bi; bi++) {
            _allocateBlock(bi);
        }
    }

    void _expand(size_t n) {
        size += n;
    }

    void _shrink(size_t n) {
        size -= n;
        _bi = size == 0 ? 0 : _blockIndexByIndex(size - 1);
    }
};

#endif // HEADER_GUARD_NK_COMMON_LOG2_ARENA
