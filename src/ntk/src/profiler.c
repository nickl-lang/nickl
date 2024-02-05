#ifdef ENABLE_PROFILING

#include "ntk/profiler.h"

#include <stdlib.h>
#include <time.h>

#include "spall.h"

static SpallProfile spall_ctx;
static _Thread_local SpallBuffer spall_buffer;
static _Thread_local uint32_t tid;

void nk_prof_init(char const *filename) {
    spall_ctx = spall_init_file(filename, 1.0);
}

void nk_prof_thread_init(uint32_t _tid, size_t buffer_size) {
    uint8_t *buffer = (uint8_t *)malloc(buffer_size);
    spall_buffer = (SpallBuffer){.data = buffer, .length = buffer_size};

    // removing initial page-fault bubbles to make the data a little more accurate, at the cost of thread spin-up time
    memset(buffer, 1, buffer_size);

    spall_buffer_init(&spall_ctx, &spall_buffer);

    tid = _tid;
}

void nk_prof_thread_exit(void) {
    spall_buffer_quit(&spall_ctx, &spall_buffer);
    free(spall_buffer.data);
}

void nk_prof_exit(void) {
    spall_quit(&spall_ctx);
}

static double getTimeUs(void) {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
}

void nk_prof_begin_block(char const *name, size_t name_len) {
    spall_buffer_begin_ex(&spall_ctx, &spall_buffer, name, name_len, getTimeUs(), tid, 0);
}

void nk_prof_end_block(void) {
    spall_buffer_end_ex(&spall_ctx, &spall_buffer, getTimeUs(), tid, 0);
}

#endif // ENABLE_PROFILING
