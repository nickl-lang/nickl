#include "ntk/os/mem.h"

#include <sys/mman.h>

void *nk_mem_reserveAndCommit(usize len) {
    return mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

i32 nk_mem_release(void *addr, usize len) {
    return munmap(addr, len);
}
