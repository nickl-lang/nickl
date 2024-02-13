#ifndef NTK_FILE_H_
#define NTK_FILE_H_

#include "ntk/allocator.h"
#include "ntk/os/file.h"
#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO Refactor NkFileReadResult
typedef struct {
    NkString bytes;
    bool ok;
} NkFileReadResult;

NkFileReadResult nk_file_read(NkAllocator alloc, NkString file);

NkStream nk_file_getStream(NkOsHandle h_file);

#ifdef __cplusplus
}
#endif

#endif // NTK_FILE_H_
