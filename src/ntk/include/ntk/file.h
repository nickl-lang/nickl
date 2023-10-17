#ifndef HEADER_GUARD_NTK_FILE
#define HEADER_GUARD_NTK_FILE

#include "ntk/allocator.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/sys/file.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nks bytes;
    bool ok;
} NkFileReadResult;

NkFileReadResult nk_file_read(NkAllocator alloc, nks file);

nk_stream nk_file_getStream(nkfd_t fd);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_FILE
