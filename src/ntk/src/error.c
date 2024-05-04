#include "ntk/error.h"

#include "ntk/list.h"
#include "ntk/string_builder.h"

static _Thread_local NkErrorState *g_error_state;

void nk_error_pushState(NkErrorState *state) {
    nk_list_push(g_error_state, state);
}

void nk_error_popState(void) {
    nk_assert(g_error_state && "no error state");

    nk_list_pop(g_error_state);
}

void nk_error_freeState(void) {
    nk_assert(g_error_state && "no error state");

    NkAllocator alloc = g_error_state->alloc.proc ? g_error_state->alloc : nk_default_allocator;

    while (g_error_state->errors) {
        NkErrorNode const *node = g_error_state->errors;
        g_error_state->errors = node->next;

        nk_free(alloc, (void *)node->msg.data, node->msg.size);
        nk_freeT(alloc, node, NkErrorNode);
    }
}

usize nk_error_count(void) {
    nk_assert(g_error_state && "no error state");

    return g_error_state->error_count;
}

i32 nk_error_printf(char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    i32 res = nk_error_vprintf(fmt, ap);
    va_end(ap);

    return res;
}

i32 nk_error_vprintf(char const *fmt, va_list ap) {
    // TODO: Handle uninitialized case
    nk_assert(g_error_state && "no error state");

    NkAllocator alloc = g_error_state->alloc.proc ? g_error_state->alloc : nk_default_allocator;

    NkErrorNode *node = nk_allocT(alloc, NkErrorNode);
    *node = (NkErrorNode){
        .next = NULL,
        .msg = {0},
    };

    if (!g_error_state->errors) {
        g_error_state->errors = g_error_state->last_error = node;
    } else {
        g_error_state->last_error->next = node;
        g_error_state->last_error = node;
    }
    g_error_state->error_count++;

    NkStringBuilder sb = (NkStringBuilder){NKSB_INIT(alloc)};
    i32 res = nksb_vprintf(&sb, fmt, ap);
    sb.data = nk_realloc(alloc, sb.size, sb.data, sb.capacity);
    node->msg = (NkString){NKS_INIT(sb)};

    return res;
}
