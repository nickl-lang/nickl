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

NK_EXPORT void nk_prof_begin(NkString name);
NK_EXPORT void nk_prof_end(void);

#ifdef __cplusplus
}
#endif

#ifdef ENABLE_PROFILING

#define NK_PROF_START(filename) nk_prof_start(filename)
#define NK_PROF_FINISH() nk_prof_finish()

#define NK_PROF_THREAD_ENTER(tid, buffer_size) nk_prof_threadEnter((tid), (buffer_size))
#define NK_PROF_THREAD_LEAVE() nk_prof_threadLeave()

#define NK_PROF_SCOPE_BEGIN(str) nk_prof_begin(str)
#define NK_PROF_FUNC_BEGIN() NK_PROF_SCOPE_BEGIN(nk_cs2s(__func__))
#define NK_PROF_END() nk_prof_end()

#ifdef __cplusplus

struct _NkProfScopeGuard {
    NK_FORCEINLINE _NkProfScopeGuard(NkString str) {
        NK_PROF_SCOPE_BEGIN(str);
    }

    NK_FORCEINLINE ~_NkProfScopeGuard() {
        NK_PROF_END();
    }
};

#define NK_PROF_SCOPE(str)                            \
    _NkProfScopeGuard NK_CAT(_prof_guard, __LINE__) { \
        {                                             \
            NK_SLICE_INIT(str)                        \
        }                                             \
    }

#define NK_PROF_FUNC() NK_PROF_SCOPE(nk_cs2s(__func__))

#else // __cplusplus

#define NK_PROF_SCOPE(str) NK_DEFER_LOOP(NK_PROF_SCOPE_BEGIN(str), NK_PROF_END())
#define NK_PROF_FUNC() NK_DEFER_LOOP(NK_PROF_FUNC_BEGIN(), NK_PROF_END())

#endif // __cplusplus

#else // ENABLE_PROFILING

#define NK_PROF_START(filename) _NK_NOP
#define NK_PROF_FINISH() _NK_NOP

#define NK_PROF_THREAD_ENTER(tid, buffer_size) _NK_NOP
#define NK_PROF_THREAD_LEAVE() _NK_NOP

#define NK_PROF_SCOPE_BEGIN(str) _NK_NOP
#define NK_PROF_FUNC_BEGIN() _NK_NOP
#define NK_PROF_END() _NK_NOP

#ifdef __cplusplus

#define NK_PROF_SCOPE(str) _NK_NOP
#define NK_PROF_FUNC() _NK_NOP

#else // __cplusplus

#define NK_PROF_SCOPE(str) if (1)
#define NK_PROF_FUNC() if (1)

#endif // __cplusplus

#endif // ENABLE_PROFILING

#endif // NTK_PROFILER_H_
