#ifndef NTK_SYSCALL_H_
#define NTK_SYSCALL_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define NK_SYSCALLS_AVAILABLE 0
#elif defined(__APPLE__)
#define NK_SYSCALLS_AVAILABLE 0
#else
#define NK_SYSCALLS_AVAILABLE 1
#endif

#if NK_SYSCALLS_AVAILABLE

NK_EXPORT long nk_syscall0(long n);
NK_EXPORT long nk_syscall1(long n, long a1);
NK_EXPORT long nk_syscall2(long n, long a1, long a2);
NK_EXPORT long nk_syscall3(long n, long a1, long a2, long a3);
NK_EXPORT long nk_syscall4(long n, long a1, long a2, long a3, long a4);
NK_EXPORT long nk_syscall5(long n, long a1, long a2, long a3, long a4, long a5);
NK_EXPORT long nk_syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6);

#endif // NK_SYSCALLS_AVAILABLE

#ifdef __cplusplus
}
#endif

#endif // NTK_SYSCALL_H_
