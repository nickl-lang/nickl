#ifndef HEADER_GUARD_NK_COMMON_SEQUENCE
#define HEADER_GUARD_NK_COMMON_SEQUENCE

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "nk/common/array.hpp"

template <class T>
struct Sequence {
    using _block_ptr = T *;

    size_t size;

    Array<_block_ptr> _block_table;
    size_t _bi; // block index
    size_t _ic; // initial capacity

    static Sequence create(size_t cap = INIT_CAPACITY) {
        Sequence seq;
        seq.init(cap);
        return seq;
    }

    void init(size_t cap = INIT_CAPACITY) {
        size = 0;
        _block_table.init(INIT_BLOCK_TAB_CAPACITY);
        _bi = 0;
        _ic = ceilToPowerOf2(cap);

        _allocateBlock(0);
    }

    void deinit() {
        _block_ptr *pblock = _block_table.data;
        for (_block_ptr *end = pblock + _block_table.size; pblock != end; pblock++) {
            _mctx.def_allocator->free(*pblock);
        }

        _block_table.deinit();
    }

    T &push(size_t n = 1) {
        EASY_BLOCK("Sequence::push", profiler::colors::Grey200)

        if (!enoughSpace(n)) {
            _expand(n);
        }

        size += n;
        T *res = _top() - n;

        LOG_USE_SCOPE(seq);
        LOG_TRC("push(size=%lu) -> %p", n * sizeof(T), res);

        return *res;
    }

    T &pop(size_t n = 1) {
        EASY_BLOCK("Sequence::pop", profiler::colors::Grey200)

        assert(size >= n && "trying to pop more bytes than available");

        size -= n;
        _bi = size == 0 ? 0 : _blockIndexByIndex(size - 1);

        T *res = _top();

        LOG_USE_SCOPE(seq);
        LOG_TRC("push(size=%lu) -> %p", n * sizeof(T), res);

        return *res;
    }

    T &at(size_t i) const {
        return *(_blockByIndex(i) + _indexRemainder(i, _ic));
    }

    bool enoughSpace(size_t n) const {
        int64_t const rem = size == 0 ? -1 : (int64_t)_indexRemainder(size - 1, _ic);
        return rem + n < _blockDataSize(_bi, _ic);
    }

    void clear() {
        pop(size);
    }

private:
    static constexpr size_t INIT_BLOCK_TAB_CAPACITY = 8;
    static constexpr size_t INIT_CAPACITY = 64;

    static size_t _indexRemainder(size_t i, size_t ic) {
        return i + ic - floorToPowerOf2(i + ic);
    }

    static size_t _precedingBlocksSize(size_t bi, size_t ic) {
        assert(bi > 0 && "no blocks precede the first one");
        return ((1ul << bi) - 1) * ic;
    }

    size_t _blockIndexByIndex(size_t i) const {
        return log2u(i / _ic + 1);
    }

    _block_ptr _blockByIndex(size_t i) const {
        return _block_table[_blockIndexByIndex(i)];
    }

    static size_t _blockDataSize(size_t bi, size_t ic) {
        return ic * (1ul << bi);
    }

    void _allocateBlock(size_t bi) {
        _block_table.push() =
            (_block_ptr)_mctx.def_allocator->alloc(_blockDataSize(bi, _ic) * sizeof(T));
    }

    void _expand(size_t n) {
        size_t const old_bi = _bi;
        size_t const new_bi = maxu(log2u(ceilToPowerOf2(n) / _ic), old_bi + 1);

        _bi = new_bi;
        size = _precedingBlocksSize(new_bi, _ic);

        for (size_t bi = _block_table.size; bi <= new_bi; bi++) {
            _allocateBlock(bi);
        }
    }

    T *_top() const {
        return size == 0 ? &at(0) : &at(size - 1) + 1;
    }
};

#endif // HEADER_GUARD_NK_COMMON_SEQUENCE
