#include "nk/common/stack_allocator.h"

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

static void _allocateBlock(StackAllocator *self, size_t n) {
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

void *StackAllocator_push(StackAllocator *self, size_t n) {
    if (n) {
        StackAllocator_reserve(self, n);
        self->_last_block->size += n;
        self->size += n;
        return (uint8_t *)_blockData(self->_last_block) + self->_last_block->size - n;
    } else {
        return NULL;
    }
}

void StackAllocator_pop(StackAllocator *self, size_t n) {
    /// @TODO StackAllocator_pop incomplete
    self->_last_block->size -= n;
    self->size -= n;
}

void StackAllocator_clear(StackAllocator *self) {
    StackAllocator_pop(self, self->size);
}

void StackAllocator_reserve(StackAllocator *self, size_t n) {
    if (n > _spaceLeftInBlock(self->_last_block)) {
        _allocateBlock(self, n);
    }
}

void StackAllocator_copy(StackAllocator const *self, void *dst) {
    size_t offset = 0;
    for (_BlockHeader *block = self->_first_block; block; block = block->next) {
        memcpy((uint8_t *)dst + offset, _blockData(block), block->size);
        offset += block->size;
    }
}

void StackAllocator_deinit(StackAllocator *self) {
    for (_BlockHeader *block = self->_first_block; block;) {
        _BlockHeader *cur_block = block;
        block = block->next;
        free(cur_block);
    }

    *self = (StackAllocator){0};
}
