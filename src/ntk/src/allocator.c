#include "ntk/allocator.h"

#include <stdlib.h>

#include "ntk/common.h"
#include "ntk/log.h"

NK_LOG_USE_SCOPE(mem);

static void *defaultAllocatorProc(
    void *data,
    NkAllocatorMode mode,
    usize size,
    u8 align,
    void *old_mem,
    usize old_size) {
    (void)data;
    (void)align;
    (void)old_size;

    switch (mode) {
        case NkAllocatorMode_Alloc: {
            void *data = malloc(size);
            NK_LOG_TRC("malloc(%" PRIu64 ") -> %p", size, data);
            if (!data) {
                NK_LOG_ERR("Out of memory");
                nk_trap();
            }
            return data;
        }

        case NkAllocatorMode_Free: {
            NK_LOG_TRC("free(%p)", old_mem);
            free(old_mem);
            return NULL;
        }

        case NkAllocatorMode_Realloc: {
            void *data = realloc(old_mem, size);
            NK_LOG_TRC("realloc(%" PRIu64 ", %p) -> %p", size, old_mem, data);
            if (!data) {
                NK_LOG_ERR("Out of memory");
                nk_trap();
            }
            return data;
        }

        case NkAllocatorMode_QuerySpaceLeft: {
            *(NkAllocatorSpaceLeftQueryResult *)old_mem = (NkAllocatorSpaceLeftQueryResult){
                .kind = NkAllocatorSpaceLeftQueryResultKind_Unknown,
                .bytes_left = 0,
            };
            return NULL;
        }

        default:
            nk_assert(!"unreachable");
            return NULL;
    }
}

NkAllocator nk_default_allocator = {
    .data = NULL,
    .proc = defaultAllocatorProc,
};
