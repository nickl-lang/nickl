#ifdef ENABLE_PROFILING

#include "ntk/profiler.h"

#include <spall.h>
#include <string.h>
#include <x86intrin.h>

#include "ntk/sys/time.h"

static SpallProfile spall_ctx;
static _Thread_local SpallBuffer spall_buffer;
static _Thread_local uint32_t tid;
static _Thread_local bool is_thread_running;

void nk_prof_init(char const *filename) {
    double timestamp_unit = 1000000.0 / (double)nk_getTscFreq();
    spall_ctx = spall_init_file(filename, timestamp_unit);
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

#endif // ENABLE_PROFILING
