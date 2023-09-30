#ifndef HEADER_GUARD_NK_COMMON_FILE
#define HEADER_GUARD_NK_COMMON_FILE

#include "nk/common/allocator.h"
#include "nk/common/stream.h"
#include "nk/common/string.h"
#include "nk/sys/file.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nkstr bytes;
    bool ok;
} NkFileReadResult;

NkFileReadResult nk_file_read(NkAllocator alloc, nkstr file);

nk_stream nk_file_openStream(nkstr file, int flags);
void nk_file_closeStream(nk_stream stream);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_FILE
