#ifndef NTK_PROFILER_H_
#define NTK_PROFILER_H_

#include "ntk/common.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_EXPORT void nk_prof_start(char const *filename);
NK_EXPORT void nk_prof_threadEnter(u32 tid, usize buffer_size);
NK_EXPORT void nk_prof_threadLeave(void);
NK_EXPORT void nk_prof_finish(void);

NK_EXPORT void nk_prof_scopeBegin(NkString name);
NK_EXPORT void nk_prof_scopeEnd(void);

#ifdef __cplusplus
}
#endif

#ifdef ENABLE_PROFILING

#define NK_PROF_START(filename) nk_prof_start(filename)
#define NK_PROF_FINISH() nk_prof_finish()

#define NK_PROF_THREAD_ENTER(tid, buffer_size) nk_prof_threadEnter((tid), (buffer_size))
#define NK_PROF_THREAD_LEAVE() nk_prof_threadLeave()

#define NK_PROF_SCOPE_BEGIN(str) nk_prof_scopeBegin(str)
#define NK_PROF_SCOPE_END() nk_prof_scopeEnd()

#define NK_PROF_FUNC_BEGIN() NK_PROF_SCOPE_BEGIN(nk_cs2s(__func__))
#define NK_PROF_FUNC_END() NK_PROF_SCOPE_END()

#ifdef __cplusplus

struct _NkProfScopeGuard {
    NK_FORCEINLINE _NkProfScopeGuard(NkString str) {
        NK_PROF_SCOPE_BEGIN(str);
    }

    NK_FORCEINLINE ~_NkProfScopeGuard() {
        NK_PROF_SCOPE_END();
    }
};

#define NK_PROF_SCOPE(str)                            \
    _NkProfScopeGuard NK_CAT(_prof_guard, __LINE__) { \
        {                                             \
            NK_SLICE_INIT(str)                        \
        }                                             \
    }

#define NK_PROF_FUNC() NK_PROF_SCOPE(nk_cs2s(__func__))

#endif // __cplusplus

#else // ENABLE_PROFILING

#define NK_PROF_START(filename) _NK_NOP
#define NK_PROF_FINISH() _NK_NOP

#define NK_PROF_THREAD_ENTER(tid, buffer_size) _NK_NOP
#define NK_PROF_THREAD_LEAVE() _NK_NOP

#define NK_PROF_SCOPE_BEGIN(str) _NK_NOP
#define NK_PROF_SCOPE_END() _NK_NOP

#define NK_PROF_FUNC_BEGIN() _NK_NOP
#define NK_PROF_FUNC_END() _NK_NOP

#ifdef __cplusplus

#define NK_PROF_SCOPE(str) _NK_NOP
#define NK_PROF_FUNC() _NK_NOP

#endif // __cplusplus

#endif // ENABLE_PROFILING

#endif // NTK_PROFILER_H_
