#include "ntk/os/time.h"

#include <sys/sysctl.h>
#include <unistd.h>

#include "ntk/time.h"

u64 nk_getTscFreq(void) {
    u64 tsc_freq;
    usize len = sizeof(tsc_freq);
    if (sysctlbyname("machdep.tsc.frequency", &tsc_freq, &len, NULL, 0) == 0) { // NOTE: x86 only!
        return tsc_freq;
    } else {
        return nk_estimateTscFrequency();
    }
}

void nk_usleep(u64 usec) {
    usleep(usec);
}
