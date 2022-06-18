#ifndef HEADER_GUARD_NK_COMMON_LOG2_ARENA
#define HEADER_GUARD_NK_COMMON_LOG2_ARENA

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "nk/common/array.hpp"

template <class T>
struct Log2Arena {
    using _block_ptr = T *;

    size_t size;

    Array<_block_ptr> _block_table;
    size_t _bi; // block index
    size_t _ic; // initial capacity

    /// @TODO Rewrite Log2Arena to suppoert zero init
    void reserve(size_t cap = INIT_CAPACITY) {
        size = 0;
        _block_table.reserve(INIT_BLOCK_TAB_CAPACITY);
        _bi = 0;
        _ic = ceilToPowerOf2(cap);

        _allocateBlock(0);
    }

    void deinit() {
        _block_ptr *pblock = _block_table.data;
        for (_block_ptr *end = pblock + _block_table.size; pblock != end; pblock++) {
            platform_free(*pblock);
        }

        _block_table.deinit();
    }

    Slice<T> push(size_t n = 1) {
        EASY_BLOCK("Log2Arena::push", profiler::colors::Grey200)

        if (!enoughSpace(n)) {
            _expand(n);
        }

        size += n;
        T *res = _top() - n;

        LOG_USE_SCOPE(log2arena);
        LOG_TRC("push(size=%lu) -> %p", n * sizeof(T), res);

        return {res, n};
    }

    Slice<T> pop(size_t n = 1) {
        EASY_BLOCK("Log2Arena::pop", profiler::colors::Grey200)

        assert(size >= n && "trying to pop more bytes than available");

        size -= n;
        _bi = size == 0 ? 0 : _blockIndexByIndex(size - 1);

        T *res = _top();

        LOG_USE_SCOPE(log2arena);
        LOG_TRC("push(size=%lu) -> %p", n * sizeof(T), res);

        return {res, n};
    }

    T &at(size_t i) const {
        return *(_blockByIndex(i) + _indexRemainder(i));
    }

    bool enoughSpace(size_t n) const {
        int64_t const rem = size == 0 ? -1 : (int64_t)_indexRemainder(size - 1);
        return rem + n < _blockDataSize(_bi);
    }

    void clear() {
        pop(size);
    }

    void append(Slice<T const> slice) {
        slice.copy(push(slice.size));
    }

private:
    static constexpr size_t INIT_BLOCK_TAB_CAPACITY = 8;
    static constexpr size_t INIT_CAPACITY = 64;

    size_t _indexRemainder(size_t i) const {
        return i + _ic - floorToPowerOf2(i + _ic);
    }

    size_t _precedingBlocksSize(size_t bi) const {
        assert(bi > 0 && "no blocks precede the first one");
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
        _block_table.push()[0] = (_block_ptr)platform_alloc(_blockDataSize(bi) * sizeof(T));
    }

    void _expand(size_t n) {
        size_t const old_bi = _bi;
        size_t const new_bi = maxu(log2u(ceilToPowerOf2(n) / _ic), old_bi + 1);

        _bi = new_bi;
        size = _precedingBlocksSize(new_bi);

        for (size_t bi = _block_table.size; bi <= new_bi; bi++) {
            _allocateBlock(bi);
        }
    }

    T *_top() const {
        return size == 0 ? &at(0) : &at(size - 1) + 1;
    }
};

#endif // HEADER_GUARD_NK_COMMON_LOG2_ARENA
