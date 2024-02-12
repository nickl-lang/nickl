#include "ntk/profiler.h"

#include <spall.h>
#include <string.h>
#include <x86intrin.h>

#include "ntk/os/time.h"

static SpallProfile spall_ctx;
static _Thread_local SpallBuffer spall_buffer;
static _Thread_local u32 tid;
static _Thread_local bool is_thread_running;

void nk_prof_start(char const *filename) {
    f64 timestamp_unit = 1000000.0 / (f64)nk_getTscFreq();
    spall_ctx = spall_init_file(filename, timestamp_unit);
}

void nk_prof_threadEnter(u32 _tid, usize buffer_size) {
    u8 *buffer = (u8 *)malloc(buffer_size);
    spall_buffer = (SpallBuffer){.data = buffer, .length = buffer_size};

    // removing initial page-fault bubbles to make the data a little more accurate, at the cost of thread spin-up time
    memset(buffer, 1, buffer_size);

    spall_buffer_init(&spall_ctx, &spall_buffer);

    tid = _tid;
    is_thread_running = true;
}

void nk_prof_threadLeave(void) {
    is_thread_running = false;
    spall_buffer_quit(&spall_ctx, &spall_buffer);
    free(spall_buffer.data);
}

void nk_prof_finish(void) {
    spall_quit(&spall_ctx);
}

void nk_prof_scopeBegin(NkString name) {
    if (is_thread_running) {
        spall_buffer_begin_ex(&spall_ctx, &spall_buffer, name.data, name.size, __rdtsc(), tid, 0);
    }
}

void nk_prof_scopeEnd(void) {
    if (is_thread_running) {
        spall_buffer_end_ex(&spall_ctx, &spall_buffer, __rdtsc(), tid, 0);
    }
}
