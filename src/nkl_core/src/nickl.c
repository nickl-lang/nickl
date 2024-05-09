#include "nkl/core/nickl.h"

#include "nickl_impl.h"
#include "nkl/common/ast.h"
#include "nkl/core/types.h"
#include "nodes.h"
#include "ntk/allocator.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/file.h"
#include "ntk/list.h"
#include "ntk/os/error.h"
#include "ntk/string_builder.h"

static u64 nk_atom_hash(NkAtom const key) {
    return nk_hashVal(key);
}
static bool nk_atom_equal(NkAtom const lhs, NkAtom const rhs) {
    return lhs == rhs;
}
static NkAtom const *Source_kv_GetKey(Source_kv const *item) {
    return &item->key;
}
NK_HASH_TREE_IMPL(FileMap, Source_kv, NkAtom, Source_kv_GetKey, nk_atom_hash, nk_atom_equal);

NklState nkl_state_create(NklLexerProc lexer_proc, NklParserProc parser_proc) {
#define XN(N, T) nk_atom_define(NK_CAT(n_, N), nk_cs2s(T));
#include "nodes.inl"

    NkArena arena = {0};

    NklState nkl = nk_arena_allocT(&arena, NklState_T);
    *nkl = (NklState_T){
        .permanent_arena = arena,
        .lexer_proc = lexer_proc,
        .parser_proc = parser_proc,
        .files = {0},
    };
    nkl->files.alloc = nk_arena_getAllocator(&nkl->permanent_arena);

    nkl_types_init(nkl);

    return nkl;
}

void nkl_state_free(NklState nkl) {
    nkl_types_free(nkl);

    NkArena arena = nkl->permanent_arena;
    nk_arena_free(&arena);
}

NklSource nkl_getSource(NklState nkl, NkAtom file) {
    Source_kv *found = FileMap_find(&nkl->files, file);
    if (!found) {
        found = FileMap_insert(&nkl->files, (Source_kv){.key = file, .val = {0}});

        NklSource *src = &found->val;
        src->file = file;

        NkAllocator alloc = nk_arena_getAllocator(&nkl->permanent_arena);

        NkString filename = nk_atom2s(file);

        NkFileReadResult read_res = nk_file_read(alloc, filename);
        if (!read_res.ok) {
            nkl_reportError(
                NK_ATOM_INVALID,
                NULL,
                "failed to read file `" NKS_FMT "`: %s",
                NKS_ARG(filename),
                nk_getLastErrorString());
        } else {
            src->text = read_res.bytes;

            src->tokens = nkl->lexer_proc(nkl, alloc, file, src->text);
            if (!nkl_getErrorCount()) {
                src->nodes = nkl->parser_proc(nkl, alloc, file, src->text, src->tokens);
            }
        }
    }
    return found->val;
}

static _Thread_local NklErrorState *g_error_state;

void nkl_errorStateEquip(NklErrorState *state) {
    nk_list_push(g_error_state, state);
}

void nkl_errorStateUnequip(void) {
    nk_assert(g_error_state && "no error state");

    nk_list_pop(g_error_state);
}

usize nkl_getErrorCount(void) {
    nk_assert(g_error_state && "no error state");

    return g_error_state->error_count;
}

NK_PRINTF_LIKE(3, 4) i32 nkl_reportError(NkAtom file, NklToken const *token, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    i32 res = nkl_vreportError(file, token, fmt, ap);
    va_end(ap);

    return res;
}

i32 nkl_vreportError(NkAtom file, NklToken const *token, char const *fmt, va_list ap) {
    nk_assert(g_error_state && "no error state");

    if (!token) {
        static NklToken const dummy_token = {0};
        token = &dummy_token;
    }

    NklError *node = nk_arena_allocT(g_error_state->arena, NklError);
    *node = (NklError){
        .next = NULL,
        .msg = {0},
        .file = file,
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
