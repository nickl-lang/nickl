#ifndef HEADER_GUARD_NK_COMMON_FILE
#define HEADER_GUARD_NK_COMMON_FILE

#include "nk/common/allocator.h"
#include "nk/common/string.h"

typedef struct {
    nkstr bytes;
    bool ok;
} NkReadFileResult;

NkReadFileResult nk_readFile(NkAllocator alloc, nkstr path);

#endif // HEADER_GUARD_NK_COMMON_FILE
