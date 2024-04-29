#include "nkl/core/nickl.h"

#include "nkl/core/types.h"
#include "nodes.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/string_builder.h"

void nkl_state_init(NklState *nkl, NklLexer lexer, NklParser parser) {
#define XN(N, T) nk_atom_define(NK_CAT(n_, N), nk_cs2s(T));
#include "nodes.inl"

    *nkl = (NklState){
        .lexer = lexer,
        .parser = parser,
    };

    nkl_types_init(nkl);
}

void nkl_state_free(NklState *nkl) {
    nkl_types_free(nkl);
}

static _Thread_local NklErrorState *g_error_state;

void nkl_errorStateInitAndEquip(NklErrorState *state, NkArena *arena) {
    nk_assert(!g_error_state && "error state is already initialized");
    *state = (NklErrorState){
        .arena = arena,
    };
    g_error_state = state;
}

usize nkl_getErrorCount(void) {
    nk_assert(g_error_state && "no error state");

    return g_error_state->error_count;
}

NK_PRINTF_LIKE(2, 3) i32 nkl_reportError(NklToken const *token, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    i32 res = nkl_vreportError(token, fmt, ap);
    va_end(ap);

    return res;
}

i32 nkl_vreportError(NklToken const *token, char const *fmt, va_list ap) {
    nk_assert(g_error_state && "no error state");

    if (!token) {
        static NklToken const dummy_token = (NklToken){0};
        token = &dummy_token;
    }

    NklError *node = nk_arena_allocT(g_error_state->arena, NklError);
    *node = (NklError){
        .next = NULL,
        .msg = {0},
        .token = token,
    };

    if (!g_error_state->errors) {
        g_error_state->errors = g_error_state->last_error = node;
    } else {
        g_error_state->last_error->next = node;
        g_error_state->last_error = node;
    }
    g_error_state->error_count++;

    NkStringBuilder sb = (NkStringBuilder){NKSB_INIT(nk_arena_getAllocator(g_error_state->arena))};
    i32 res = nksb_vprintf(&sb, fmt, ap);
    nk_arena_pop(g_error_state->arena, sb.capacity - sb.size);
    node->msg = (NkString){NKS_INIT(sb)};

    return res;
}
