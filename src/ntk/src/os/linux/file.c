#include "ntk/os/file.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "ntk/profiler.h"

char const *nk_null_file = "/dev/null";

i32 nk_read(NkOsHandle h_file, char *buf, usize n) {
    NK_PROF_FUNC_BEGIN();
    i32 ret = read(handle_toFd(h_file), buf, n);
    NK_PROF_FUNC_END();
    return ret;
}

i32 nk_write(NkOsHandle h_file, char const *buf, usize n) {
    NK_PROF_FUNC_BEGIN();
    i32 ret = write(handle_toFd(h_file), buf, n);
    NK_PROF_FUNC_END();
    return ret;
}

NkOsHandle nk_open(char const *file, i32 flags) {
    NK_PROF_FUNC_BEGIN();
    i32 oflag = ((flags & NkOpenFlags_Read) && (flags & NkOpenFlags_Write) ? O_RDWR : 0) |
                ((flags & NkOpenFlags_Read) && !(flags & NkOpenFlags_Write) ? O_RDONLY : 0) |
                (!(flags & NkOpenFlags_Read) && (flags & NkOpenFlags_Write) ? O_WRONLY : 0) |
                ((flags & NkOpenFlags_Create) ? O_CREAT : 0) | ((flags & NkOpenFlags_Truncate) ? O_TRUNC : 0);
    NkOsHandle ret = handle_fromFd(open(file, oflag, 0644));
    NK_PROF_FUNC_END();
    return ret;
}

i32 nk_close(NkOsHandle h_file) {
    if (!nkos_handleIsZero(h_file)) {
        NK_PROF_FUNC_BEGIN();
        i32 ret = close(handle_toFd(h_file));
        NK_PROF_FUNC_END();
        return ret;
    } else {
        return 0;
    }
}

NkOsHandle nk_stdin() {
    return handle_fromFd(0);
}

NkOsHandle nk_stdout() {
    return handle_fromFd(1);
}

NkOsHandle nk_stderr() {
    return handle_fromFd(2);
}
