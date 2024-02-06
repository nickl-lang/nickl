#ifndef HEADER_GUARD_NTK_PROFILER
#define HEADER_GUARD_NTK_PROFILER

#ifdef ENABLE_PROFILING

#include <stddef.h>
#include <stdint.h>

#include "ntk/common.h"
#include "ntk/utils.h"

#define ProfInit(filename) nk_prof_init(filename)
#define ProfExit() nk_prof_exit()

#define ProfThreadInit(tid, buffer_size) nk_prof_thread_init((tid), (buffer_size));
#define ProfThreadExit() nk_prof_thread_exit();

#define ProfBeginBlock(name, name_len) nk_prof_begin_block((name), (name_len))
#define ProfEndBlock() nk_prof_end_block()

#define ProfBeginFunc() nk_prof_begin_block(__func__, sizeof(__func__) - 1)

#ifdef __cplusplus
extern "C" {
#endif

void NK_EXPORT nk_prof_init(char const *filename);
void NK_EXPORT nk_prof_thread_init(uint32_t tid, size_t buffer_size);
void NK_EXPORT nk_prof_thread_exit(void);
void NK_EXPORT nk_prof_exit(void);

void NK_EXPORT nk_prof_begin_block(char const *name, size_t name_len);
void NK_EXPORT nk_prof_end_block(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

struct _ProfBlockGuard {
    explicit NK_FORCEINLINE _ProfBlockGuard(char const *name, size_t name_len) {
        ProfBeginBlock(name, name_len);
    }

    NK_FORCEINLINE ~_ProfBlockGuard() {
        ProfEndBlock();
    }
};

#endif // __cplusplus

#define ProfFunc() _ProfBlockGuard CAT(_prof_guard, __LINE__){__func__, sizeof(__func__) - 1};
#define ProfBlock(name, name_len) _ProfBlockGuard CAT(_prof_guard, __LINE__){(name), (name_len)};

#else // ENABLE_PROFILING

#define ProfInit(filename)
#define ProfExit()

#define ProfThreadInit(tid, buffer_size)
#define ProfThreadExit()

#define ProfBeginBlock(name, name_len)
#define ProfEndBlock()

#define ProfBeginFunc()

#define ProfFunc()
#define ProfBlock(name, name_len)

#endif // ENABLE_PROFILING

#endif // HEADER_GUARD_NTK_PROFILER
