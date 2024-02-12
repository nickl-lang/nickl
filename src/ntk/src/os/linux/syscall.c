#include "ntk/os/syscall.h"

#include <unistd.h>

nksc_t nk_syscall0(nksc_t n) {
    return syscall(n);
}

nksc_t nk_syscall1(nksc_t n, nksc_t a1) {
    return syscall(n, a1);
}

nksc_t nk_syscall2(nksc_t n, nksc_t a1, nksc_t a2) {
    return syscall(n, a1, a2);
}

nksc_t nk_syscall3(nksc_t n, nksc_t a1, nksc_t a2, nksc_t a3) {
    return syscall(n, a1, a2, a3);
}

nksc_t nk_syscall4(nksc_t n, nksc_t a1, nksc_t a2, nksc_t a3, nksc_t a4) {
    return syscall(n, a1, a2, a3, a4);
}

nksc_t nk_syscall5(nksc_t n, nksc_t a1, nksc_t a2, nksc_t a3, nksc_t a4, nksc_t a5) {
    return syscall(n, a1, a2, a3, a4, a5);
}

nksc_t nk_syscall6(nksc_t n, nksc_t a1, nksc_t a2, nksc_t a3, nksc_t a4, nksc_t a5, nksc_t a6) {
    return syscall(n, a1, a2, a3, a4, a5, a6);
}
