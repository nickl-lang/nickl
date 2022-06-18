#include "nk/common/arena.hpp"

#include <stdint.h>
#include <stdlib.h>

#include "nk/common/utils.h"

typedef struct _BlockHeader {
    struct _BlockHeader *next;
    size_t size;
    size_t capacity;
} _BlockHeader;

static size_t _align(size_t n) {
    return roundUp(n, 16);
}

static void *_blockData(_BlockHeader *block) {
    return (void *)_align((size_t)(block + 1));
}

static size_t _spaceLeftInBlock(_BlockHeader const *block) {
    return block ? block->capacity - block->size : 0;
}

static void _allocateBlock(Arena *self, size_t n) {
    size_t const header_size = _align(sizeof(_BlockHeader));
    n = ceilToPowerOf2(n + header_size);
    n = maxu(n, (self->_last_block ? self->_last_block->capacity << 1 : 0));

    _BlockHeader *block = (_BlockHeader *)malloc(n);

    block->next = NULL;
    block->size = 0;
    block->capacity = n - header_size;

    if (!self->_last_block) {
        self->_first_block = block;
        self->_last_block = block;
    } else {
        self->_last_block->next = block;
    }

    self->_last_block = block;
}

void *Arena_push(Arena *self, size_t n) {
    if (n) {
        Arena_reserve(self, n);
        self->_last_block->size += n;
        self->size += n;
        return (uint8_t *)_blockData(self->_last_block) + self->_last_block->size - n;
    } else {
        return NULL;
    }
}

void Arena_pop(Arena *self, size_t n) {
    /// @TODO Arena_pop incomplete
    self->_last_block->size -= n;
    self->size -= n;
}

void Arena_clear(Arena *self) {
    Arena_pop(self, self->size);
}

void Arena_reserve(Arena *self, size_t n) {
    if (n > _spaceLeftInBlock(self->_last_block)) {
        _allocateBlock(self, n);
    }
}

void Arena_copy(Arena const *self, void *dst) {
    size_t offset = 0;
    for (_BlockHeader *block = self->_first_block; block; block = block->next) {
        memcpy((uint8_t *)dst + offset, _blockData(block), block->size);
        offset += block->size;
    }
}

void Arena_deinit(Arena *self) {
    for (_BlockHeader *block = self->_first_block; block;) {
        _BlockHeader *cur_block = block;
        block = block->next;
        free(cur_block);
    }

    *self = (Arena){0};
}
