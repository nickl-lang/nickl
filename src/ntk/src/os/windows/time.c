#include "ntk/time.h"

#include <unistd.h>
#include <x86intrin.h>

#include "common.h"
#include "ntk/time.h"

u64 nk_getTscFreq(void) {
    return nk_estimateTscFrequency();
}

void nk_usleep(u64 usec) {
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10*usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}

u64 nk_readTsc(void) {
    return __rdtsc();
}
