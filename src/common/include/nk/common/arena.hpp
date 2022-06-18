#ifndef HEADER_GUARD_NK_COMMON_ARENA
#define HEADER_GUARD_NK_COMMON_ARENA

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _BlockHeader;

typedef struct {
    size_t size;
    struct _BlockHeader *_first_block;
    struct _BlockHeader *_last_block;
} Arena;

void Arena_reserve(Arena *self, size_t n);

void *Arena_push(Arena *self, size_t n);
void Arena_pop(Arena *self, size_t n);
void Arena_clear(Arena *self);

void Arena_copy(Arena const *self, void *dst);

void Arena_deinit(Arena *self);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_ARENA
