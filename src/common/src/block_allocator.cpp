#include "nk/common/block_allocator.h"

#include <stdarg.h>
#include <stdlib.h>

#include "nk/common/utils.hpp"

struct _BlockHeader {
    _BlockHeader *next;
    size_t size;
    size_t capacity;
};

static size_t _align(size_t n) {
    return roundUp(n, 16);
}

static uint8_t *_blockData(_BlockHeader *block) {
    return (uint8_t *)_align((size_t)(block + 1));
}

static size_t _spaceLeftInBlock(_BlockHeader const *block) {
    return block ? block->capacity - block->size : 0;
}

static void _allocateBlock(BlockAllocator *self, size_t n) {
    auto const header_size = _align(sizeof(_BlockHeader));
    n = ceilToPowerOf2(n + header_size);
    n = maxu(n, (self->_last_block ? self->_last_block->capacity << 1 : 0));

    _BlockHeader *block = (_BlockHeader *)malloc(n);

    block->next = nullptr;
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

uint8_t *BlockAllocator_push(BlockAllocator *self, size_t n) {
    if (n) {
        BlockAllocator_reserve(self, n);
        self->_last_block->size += n;
        self->size += n;
        return _blockData(self->_last_block) + self->_last_block->size - n;
    } else {
        return nullptr;
    }
}

void BlockAllocator_reserve(BlockAllocator *self, size_t n) {
    if (n > _spaceLeftInBlock(self->_last_block)) {
        _allocateBlock(self, n);
    }
}

void BlockAllocator_copy(BlockAllocator const *self, uint8_t *dst) {
    size_t offset = 0;
    for (_BlockHeader *block = self->_first_block; block; block = block->next) {
        memcpy(dst + offset, _blockData(block), block->size);
        offset += block->size;
    }
}

void BlockAllocator_deinit(BlockAllocator *self) {
    for (_BlockHeader *block = self->_first_block; block;) {
        _BlockHeader *cur_block = block;
        block = block->next;
        free(cur_block);
    }

    *self = {};
}
