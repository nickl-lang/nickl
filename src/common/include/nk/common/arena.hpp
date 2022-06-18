#ifndef HEADER_GUARD_NK_COMMON_ARENA
#define HEADER_GUARD_NK_COMMON_ARENA

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "nk/common/slice.hpp"
#include "nk/common/utils.h"

template <class T>
struct Arena {
    struct _BlockHeader {
        _BlockHeader *next;
        size_t size;
        size_t capacity;
    };

    size_t size;

    _BlockHeader *_first_block;
    _BlockHeader *_last_block;

    void reserve(size_t n) {
        if (enoughSpace(n)) {
            _allocateBlock(n);
        }
    }

    void deinit() {
        for (_BlockHeader *block = _first_block; block;) {
            _BlockHeader *cur_block = block;
            block = block->next;
            free(cur_block);
        }

        *this = {};
    }

    Slice<T> push(size_t n = 1) {
        if (n) {
            reserve(n);
            _last_block->size += n;
            size += n;
            return {_blockData(_last_block) + _last_block->size - n, n};
        } else {
            /// @TODO Return top when pushing zero?
            return {};
        }
    }

    /// @TODO Arena::pop incomplete
    Slice<T> pop(size_t n = 1) {
        _last_block->size -= n;
        size -= n;
        return {};
    }

    bool enoughSpace(size_t n) const {
        return n > _spaceLeftInBlock(_last_block);
    }

    void clear() {
        pop(size);
    }

    void copy(Slice<std::decay_t<T>> dst) {
        assert(dst.size >= size && "copying to a slice of insufficient size");
        copy(dst.data);
    }

    void copy(std::decay_t<T> *dst) const {
        size_t offset = 0;
        for (_BlockHeader *block = _first_block; block; block = block->next) {
            Slice<T const>{_blockData(block), block->size}.copy(dst + offset);
            offset += block->size;
        }
    }

    void append(Slice<T const> slice) {
        slice.copy(push(slice.size));
    }

private:
    static size_t _align(size_t n) {
        return roundUp(n, 16);
    }

    static T *_blockData(_BlockHeader *block) {
        return (T *)_align((size_t)(block + 1));
    }

    static size_t _spaceLeftInBlock(_BlockHeader const *block) {
        return block ? block->capacity - block->size : 0;
    }

    void _allocateBlock(size_t n) {
        size_t const header_size = _align(sizeof(_BlockHeader));
        n = ceilToPowerOf2(n + header_size);
        n = maxu(n, (_last_block ? _last_block->capacity << 1 : 0));

        _BlockHeader *block = (_BlockHeader *)malloc(n);

        block->next = nullptr;
        block->size = 0;
        block->capacity = n - header_size;

        if (!_last_block) {
            _first_block = block;
            _last_block = block;
        } else {
            _last_block->next = block;
        }

        _last_block = block;
    }
};

#endif // HEADER_GUARD_NK_COMMON_ARENA
