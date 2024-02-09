#include "ntk/sys/mem.h"

#include <sys/mman.h>

void *nk_valloc(usize len) {
    return mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

i32 nk_vfree(void *addr, usize len) {
    return munmap(addr, len);
}
