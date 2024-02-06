#ifdef ENABLE_PROFILING

#include "ntk/profiler.h"

#include <stdlib.h>

#include "spall.h"

static SpallProfile spall_ctx;
static _Thread_local SpallBuffer spall_buffer;
static _Thread_local uint32_t tid;
static _Thread_local bool is_thread_running;

static double get_rdtsc_multiplier(void);

void nk_prof_init(char const *filename) {
    spall_ctx = spall_init_file(filename, get_rdtsc_multiplier());
}

void nk_prof_thread_init(uint32_t _tid, size_t buffer_size) {
    uint8_t *buffer = (uint8_t *)malloc(buffer_size);
    spall_buffer = (SpallBuffer){.data = buffer, .length = buffer_size};

    // removing initial page-fault bubbles to make the data a little more accurate, at the cost of thread spin-up time
    memset(buffer, 1, buffer_size);

    spall_buffer_init(&spall_ctx, &spall_buffer);

    tid = _tid;
    is_thread_running = true;
}

void nk_prof_thread_exit(void) {
    is_thread_running = false;
    spall_buffer_quit(&spall_ctx, &spall_buffer);
    free(spall_buffer.data);
}

void nk_prof_exit(void) {
    spall_quit(&spall_ctx);
}

#include <x86intrin.h>

void nk_prof_begin_block(char const *name, size_t name_len) {
    if (is_thread_running) {
        spall_buffer_begin_ex(&spall_ctx, &spall_buffer, name, name_len, __rdtsc(), tid, 0);
    }
}

void nk_prof_end_block(void) {
    if (is_thread_running) {
        spall_buffer_end_ex(&spall_ctx, &spall_buffer, __rdtsc(), tid, 0);
    }
}

#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return syscall(SYS_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

static double get_rdtsc_multiplier(void) {
    struct perf_event_attr pe = {0};
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    uint64_t tsc_freq = 0;

    int fd = perf_event_open(&pe, 0, -1, -1, 0);
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
        // Get time before sleep
        uint64_t nsc_begin = 0;
        {
            struct timespec t;
            if (!clock_gettime(CLOCK_MONOTONIC_RAW, &t)) {
                nsc_begin = (uint64_t)t.tv_sec * 1000000000ull + t.tv_nsec;
            }
        }
        uint64_t tsc_begin = __rdtsc();

        usleep(10000); // 10ms gives ~4.5 digits of precision - the longer you sleep, the more precise you get

        // Get time after sleep
        uint64_t nsc_end = nsc_begin + 1;
        {
            struct timespec t;
            if (!clock_gettime(CLOCK_MONOTONIC_RAW, &t)) {
                nsc_end = (uint64_t)t.tv_sec * 1000000000ull + t.tv_nsec;
            }
        }
        uint64_t tsc_end = __rdtsc();

        // Do the math to extrapolate the RDTSC ticks elapsed in 1 second
        tsc_freq = (tsc_end - tsc_begin) * 1000000000ull / (nsc_end - nsc_begin);
    }

    // Failure case
    if (!tsc_freq) {
        tsc_freq = 1000000000ull;
    }

    return 1000000.0 / (double)tsc_freq;
}

#endif // ENABLE_PROFILING
