#ifndef HEADER_GUARD_NK_COMMON_PROFILER
#define HEADER_GUARD_NK_COMMON_PROFILER

#include <cstdio>
#include <thread>

#if BUILD_WITH_EASY_PROFILER

#include <easy/profiler.h>

#else // BUILD_WITH_EASY_PROFILER

#define EASY_BLOCK(...)
#define EASY_NONSCOPED_BLOCK(...)
#define EASY_FUNCTION(...)
#define EASY_END_BLOCK
#define EASY_PROFILER_ENABLE
#define EASY_PROFILER_DISABLE
#define EASY_EVENT(...)
#define EASY_THREAD(...)
#define EASY_THREAD_SCOPE(...)
#define EASY_MAIN_THREAD
#define EASY_SET_EVENT_TRACING_ENABLED(isEnabled)
#define EASY_SET_LOW_PRIORITY_EVENT_TRACING(isLowPriority)

#endif // BUILD_WITH_EASY_PROFILER

struct ProfilerWrapper {
    ProfilerWrapper() {
#ifdef BUILD_WITH_EASY_PROFILER
        EASY_PROFILER_ENABLE;
        ::profiler::startListen(EASY_PROFILER_PORT);
#endif // BUILD_WITH_EASY_PROFILER
    }

    ~ProfilerWrapper() {
#ifdef BUILD_WITH_EASY_PROFILER
        ::profiler::dumpBlocksToFile("profiler_events.prof");
#endif // BUILD_WITH_EASY_PROFILER
    }
};

#endif // HEADER_GUARD_NK_COMMON_PROFILER
