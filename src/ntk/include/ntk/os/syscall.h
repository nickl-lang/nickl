#ifndef NTK_OS_SYSCALL_H_
#define NTK_OS_SYSCALL_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define NK_SYSCALLS_AVAILABLE 0
#else //_WIN32
#define NK_SYSCALLS_AVAILABLE 1
#endif //_WIN32

#if NK_SYSCALLS_AVAILABLE

long nk_syscall0(long n);
long nk_syscall1(long n, long a1);
long nk_syscall2(long n, long a1, long a2);
long nk_syscall3(long n, long a1, long a2, long a3);
long nk_syscall4(long n, long a1, long a2, long a3, long a4);
long nk_syscall5(long n, long a1, long a2, long a3, long a4, long a5);
long nk_syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6);

#endif // NK_SYSCALLS_AVAILABLE

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_SYSCALL_H_