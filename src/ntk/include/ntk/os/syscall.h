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

typedef long long nksc_t;

nksc_t nk_syscall0(nksc_t n);
nksc_t nk_syscall1(nksc_t n, nksc_t a1);
nksc_t nk_syscall2(nksc_t n, nksc_t a1, nksc_t a2);
nksc_t nk_syscall3(nksc_t n, nksc_t a1, nksc_t a2, nksc_t a3);
nksc_t nk_syscall4(nksc_t n, nksc_t a1, nksc_t a2, nksc_t a3, nksc_t a4);
nksc_t nk_syscall5(nksc_t n, nksc_t a1, nksc_t a2, nksc_t a3, nksc_t a4, nksc_t a5);
nksc_t nk_syscall6(nksc_t n, nksc_t a1, nksc_t a2, nksc_t a3, nksc_t a4, nksc_t a5, nksc_t a6);

#endif // NK_SYSCALLS_AVAILABLE

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_SYSCALL_H_
