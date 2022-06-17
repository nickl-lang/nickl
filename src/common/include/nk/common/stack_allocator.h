#ifndef HEADER_GUARD_NK_COMMON_STACK_ALLOCATOR
#define HEADER_GUARD_NK_COMMON_STACK_ALLOCATOR

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _BlockHeader;

typedef struct {
    size_t size;
    struct _BlockHeader *_first_block;
    struct _BlockHeader *_last_block;
} StackAllocator;

void StackAllocator_reserve(StackAllocator *self, size_t n);

void *StackAllocator_push(StackAllocator *self, size_t n);
void StackAllocator_pop(StackAllocator *self, size_t n);
void StackAllocator_clear(StackAllocator *self);

void StackAllocator_copy(StackAllocator const *self, void *dst);

void StackAllocator_deinit(StackAllocator *self);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_STACK_ALLOCATOR
