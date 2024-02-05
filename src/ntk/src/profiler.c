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

// #include <time.h>

// static double getTimeUs(void) {
//     struct timespec ts = {0};
//     clock_gettime(CLOCK_MONOTONIC, &ts);
//     return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
// }

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
#include <stdio.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

/*
        This is supporting code to read the RDTSC multiplier from perf on Linux,
        so we can convert from RDTSC's clock to microseconds.

        Using the RDTSC directly like this reduces profiler overhead a lot,
        which can save you a ton of time and improve the quality of your trace.

        For my i7-8559U, it takes ~20 ns to call clock_gettime, or ~6 ns to use rdtsc directly, which adds up!
*/
static uint64_t mul_u64_u32_shr(uint64_t cyc, uint32_t mult, uint32_t shift) {
    __uint128_t x = cyc;
    x *= mult;
    x >>= shift;
    return x;
}

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

static double get_rdtsc_multiplier(void) {
    struct perf_event_attr pe = {
        .type = PERF_TYPE_HARDWARE,
        .size = sizeof(struct perf_event_attr),
        .config = PERF_COUNT_HW_INSTRUCTIONS,
        .disabled = 1,
        .exclude_kernel = 1,
        .exclude_hv = 1};

    int fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd == -1) {
        perror("perf_event_open failed");
        return 1;
    }
    void *addr = mmap(NULL, 4 * 1024, PROT_READ, MAP_SHARED, fd, 0);
    if (!addr) {
        perror("mmap failed");
        return 1;
    }
    struct perf_event_mmap_page *pc = addr;
    if (pc->cap_user_time != 1) {
        fprintf(stderr, "Perf system doesn't support user time\n");
        return 1;
    }

    double nanos = (double)mul_u64_u32_shr(1000000, pc->time_mult, pc->time_shift);
    return nanos / 1000000000;
}

#endif // ENABLE_PROFILING
