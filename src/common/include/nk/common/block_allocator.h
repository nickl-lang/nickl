#ifndef HEADER_GUARD_NK_COMMON_BLOCK_ALLOCATOR
#define HEADER_GUARD_NK_COMMON_BLOCK_ALLOCATOR

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _BlockHeader;

typedef struct {
    size_t size;
    struct _BlockHeader *_first_block;
    struct _BlockHeader *_last_block;
} BlockAllocator;

void BlockAllocator_reserve(BlockAllocator *self, size_t n);
uint8_t *BlockAllocator_push(BlockAllocator *self, size_t n);
void BlockAllocator_pop(BlockAllocator *self, size_t n);
void BlockAllocator_copy(BlockAllocator const *self, uint8_t *dst);

void BlockAllocator_deinit(BlockAllocator *self);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_BLOCK_ALLOCATOR
