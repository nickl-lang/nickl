#include "ntk/time.h"

#include <linux/perf_event.h>
#include <sys/mman.h>
#include <syscall.h>
#include <unistd.h>

#include "ntk/time.h"

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, i32 cpu, i32 group_fd, unsigned long flags) {
    return syscall(SYS_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

u64 nk_getTscFreq(void) {
    struct perf_event_attr pe = {0};
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    u64 tsc_freq = 0;

    i32 fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd != -1) {
        void *addr = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
        if (addr) {
            struct perf_event_mmap_page *pc = addr;
            if (pc->cap_user_time == 1) {
                // Docs say nanoseconds = (tsc * time_mult) >> time_shift
                //      set nanoseconds = 1000000000 = 1 second in nanoseconds, solve for tsc
                //       =>         tsc = 1000000000 / (time_mult >> time_shift)
                tsc_freq =
                    (1000000000ull << (pc->time_shift / 2)) / (pc->time_mult >> (pc->time_shift - pc->time_shift / 2));
                // If your build configuration supports 128 bit arithmetic, do this:
                // tsc_freq = ((__uint128_t)1000000000ull << (__uint128_t)pc->time_shift) / pc->time_mult;
            }
            munmap(pc, 4096);
        }
        close(fd);
    }

    // Slow path
    if (!tsc_freq) {
        tsc_freq = nk_estimateTscFrequency();
    }

    return tsc_freq;
}
