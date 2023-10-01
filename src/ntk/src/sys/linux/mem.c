#include "ntk/sys/mem.h"

#include <sys/mman.h>

void *nk_valloc(size_t len) {
    return mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

int nk_vfree(void *addr, size_t len) {
    return munmap(addr, len);
}
