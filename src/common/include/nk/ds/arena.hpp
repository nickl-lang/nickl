#ifndef HEADER_GUARD_NK_COMMON_ARENA
#define HEADER_GUARD_NK_COMMON_ARENA

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "nk/ds/private/container_base.hpp"
#include "nk/ds/slice.hpp"
#include "nk/mem/mem.h"
#include "nk/utils/utils.h"

namespace nk {

template <class T>
struct Arena : ContainerBase<Arena, T> {
    friend struct ContainerBase<Arena, T>;

    struct _BlockHeader {
        _BlockHeader *next;
        size_t size;
        size_t capacity;
    };

    size_t size;

    _BlockHeader *_first_block;
    _BlockHeader *_last_block;

    void deinit() {
        for (_BlockHeader *block = _first_block; block;) {
            _BlockHeader *cur_block = block;
            block = block->next;
            nk_platform_free(cur_block);
        }

        *this = {};
    }

    bool enoughSpace(size_t n) const {
        return n <= _spaceLeftInBlock(_last_block);
    }

    Slice<std::decay_t<T>> copy(Slice<std::decay_t<T>> dst) {
        assert(dst.size >= size && "copying to a slice of insufficient size");
        return copy(dst.data);
    }

    Slice<std::decay_t<T>> copy(std::decay_t<T> *dst) const {
        size_t offset = 0;
        for (_BlockHeader *block = _first_block; block; block = block->next) {
            _blockData(block).copy(dst + offset);
            offset += block->size;
        }
        return {dst, size};
    }

private:
    T *_top() const {
        return _last_block ? _blockData(_last_block).end() : 0;
    }

    static size_t _headerSize() {
        return roundUp(sizeof(_BlockHeader), alignof(T));
    }

    static Slice<T> _blockData(_BlockHeader *block) {
        return {(T *)((uint8_t *)block + _headerSize()), block->size};
    }

    static size_t _spaceLeftInBlock(_BlockHeader const *block) {
        return block ? block->capacity - block->size : 0;
    }

    void _realloc(size_t n) {
        for (; _last_block && _last_block->next && !enoughSpace(n);
             _last_block = _last_block->next) {
        }

        if (!enoughSpace(n)) {
            n = ceilToPowerOf2(n + _headerSize());
            n = maxu(n, (_last_block ? _last_block->capacity << 1 : 0));

            auto block = new (nk_platform_alloc(n * sizeof(T))) _BlockHeader{
                .next = nullptr,
                .size = 0,
                .capacity = n - _headerSize(),
            };

            if (!_last_block) {
                _first_block = block;
            } else {
                _last_block->next = block;
            }

            _last_block = block;
        }
    }

    void _expand(size_t n) {
        if (n > 0) {
            size += n;
            _last_block->size += n;
        }
    }

    void _shrink(size_t n) {
        size -= n;
        if (n <= _last_block->size) {
            _last_block->size -= n;
        } else {
            size_t sz = size;
            _BlockHeader *block = _first_block;
            for (_last_block = block; sz; block = block->next) {
                if (block->size > sz) {
                    block->size = sz;
                }
                sz -= block->size;
                _last_block = block;
            }
            for (; block; block = block->next) {
                block->size = 0;
            }
        }
    }
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_ARENA
