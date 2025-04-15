#include "ntk/syscall.h"

#include <unistd.h>

long nk_syscall0(long n) {
    return syscall(n);
}

long nk_syscall1(long n, long a1) {
    return syscall(n, a1);
}

long nk_syscall2(long n, long a1, long a2) {
    return syscall(n, a1, a2);
}

long nk_syscall3(long n, long a1, long a2, long a3) {
    return syscall(n, a1, a2, a3);
}

long nk_syscall4(long n, long a1, long a2, long a3, long a4) {
    return syscall(n, a1, a2, a3, a4);
}

long nk_syscall5(long n, long a1, long a2, long a3, long a4, long a5) {
    return syscall(n, a1, a2, a3, a4, a5);
}

long nk_syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    return syscall(n, a1, a2, a3, a4, a5, a6);
}
