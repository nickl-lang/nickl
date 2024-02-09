#ifndef HEADER_GUARD_NTK_PROFILER
#define HEADER_GUARD_NTK_PROFILER

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ntk/common.h"
#include "ntk/string.h"
#include "ntk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

void nk_prof_init(char const *filename);
void nk_prof_thread_init(u32 tid, usize buffer_size);
void nk_prof_thread_exit(void);
void nk_prof_exit(void);

void nk_prof_begin_block(nks name);
void nk_prof_end_block(void);

#ifdef __cplusplus
}
#endif

#ifdef ENABLE_PROFILING

#define ProfInit(filename) nk_prof_init(filename)
#define ProfExit() nk_prof_exit()

#define ProfThreadInit(tid, buffer_size) nk_prof_thread_init((tid), (buffer_size));
#define ProfThreadExit() nk_prof_thread_exit();

#define ProfBeginBlock(str) nk_prof_begin_block(str)
#define ProfEndBlock() nk_prof_end_block()

#define ProfBeginFunc() ProfBeginBlock(nk_cs2s(__func__))

#ifdef __cplusplus

struct _ProfScopeGuard {
    NK_FORCEINLINE _ProfScopeGuard(nks str) {
        ProfBeginBlock(str);
    }

    NK_FORCEINLINE ~_ProfScopeGuard() {
        ProfEndBlock();
    }
};

#define ProfScope(str)                           \
    _ProfScopeGuard CAT(_prof_guard, __LINE__) { \
        { nkav_init(str) }                       \
    }

#define ProfFunc() ProfScope(nk_cs2s(__func__))

#endif // __cplusplus

#else // ENABLE_PROFILING

#define ProfInit(filename) _NK_NOP
#define ProfExit() _NK_NOP

#define ProfThreadInit(tid, buffer_size) _NK_NOP
#define ProfThreadExit() _NK_NOP

#define ProfBeginBlock(str) _NK_NOP
#define ProfEndBlock() _NK_NOP

#define ProfBeginFunc() _NK_NOP

#ifdef __cplusplus

#define ProfScope(str) _NK_NOP
#define ProfFunc() _NK_NOP

#endif // __cplusplus

#endif // ENABLE_PROFILING

#endif // HEADER_GUARD_NTK_PROFILER
