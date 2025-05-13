#include "ntk/file.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "ntk/profiler.h"

char const *nk_null_file = "/dev/null";

i32 nk_read(NkHandle file, char *buf, usize n) {
    i32 ret;
    NK_PROF_FUNC() {
        ret = read(handle2fd(file), buf, n);
    }
    return ret;
}

i32 nk_write(NkHandle file, char const *buf, usize n) {
    i32 ret;
    NK_PROF_FUNC() {
        ret = write(handle2fd(file), buf, n);
    }
    return ret;
}

i32 nk_flush(NkHandle file) {
    i32 ret;
    NK_PROF_FUNC() {
        ret = fsync(handle2fd(file));
    }
    return ret;
}

NkHandle nk_open(char const *path, i32 flags) {
    NkHandle ret;
    NK_PROF_FUNC() {
        i32 oflag = ((flags & NkOpenFlags_Read) && (flags & NkOpenFlags_Write) ? O_RDWR : 0) |
                    ((flags & NkOpenFlags_Read) && !(flags & NkOpenFlags_Write) ? O_RDONLY : 0) |
                    (!(flags & NkOpenFlags_Read) && (flags & NkOpenFlags_Write) ? O_WRONLY : 0) |
                    ((flags & NkOpenFlags_Create) ? O_CREAT : 0) | ((flags & NkOpenFlags_Truncate) ? O_TRUNC : 0);
        ret = fd2handle(open(path, oflag, 0644));
    }
    return ret;
}

i32 nk_close(NkHandle file) {
    i32 ret = 0;
    NK_PROF_FUNC() {
        if (!nk_handleIsZero(file)) {
            ret = close(handle2fd(file));
        }
    }
    return ret;
}

NkHandle nk_stdin(void) {
    return fd2handle(0);
}

NkHandle nk_stdout(void) {
    return fd2handle(1);
}

NkHandle nk_stderr(void) {
    return fd2handle(2);
}
