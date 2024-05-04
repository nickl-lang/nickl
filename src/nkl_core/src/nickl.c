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

static NklTokenArray nkl_lex(NklLexer lexer, NklState nkl, NkAllocator alloc, NkString text) {
    return lexer.proc(lexer.data, nkl, alloc, text);
}

static NklAstNodeArray nkl_parse(
    NklParser parser,
    NklState nkl,
    NkAllocator alloc,
    NkString text,
    NklTokenArray tokens) {
    return parser.proc(parser.data, nkl, alloc, text, tokens);
}

NklState nkl_state_create(NklLexer lexer, NklParser parser) {
#define XN(N, T) nk_atom_define(NK_CAT(n_, N), nk_cs2s(T));
#include "nodes.inl"

    NkArena arena = {0};

    NklState nkl = nk_arena_allocT(&arena, NklState_T);
    *nkl = (NklState_T){
        .permanent_arena = arena,
        .errors = {0},
        .lexer = lexer,
        .parser = parser,
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
        found = FileMap_insert(&nkl->files, (Source_kv){.key = file});

        NklSource *src = &found->val;

        NkAllocator alloc = nk_arena_getAllocator(&nkl->permanent_arena);

        NkString filename = nk_atom2s(file);

        NkFileReadResult read_res = nk_file_read(alloc, filename);
        if (!read_res.ok) {
            nkl_reportError(
                nkl, NULL, "failed to read file `" NKS_FMT "`: %s", NKS_ARG(filename), nk_getLastErrorString());
        } else {
            src->text = read_res.bytes;

            src->tokens = nkl_lex(nkl->lexer, nkl, alloc, src->text);
            if (!nkl_getErrorCount(nkl)) {
                src->nodes = nkl_parse(nkl->parser, nkl, alloc, src->text, src->tokens);
            }
        }
    }
    return found->val;
}

usize nkl_getErrorCount(NklState nkl) {
    return nkl->errors.error_count;
}

NklError *nkl_getErrorList(NklState nkl) {
    return nkl->errors.errors;
}

NK_PRINTF_LIKE(3, 4) i32 nkl_reportError(NklState nkl, NklToken const *token, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    i32 res = nkl_vreportError(nkl, token, fmt, ap);
    va_end(ap);

    return res;
}

i32 nkl_vreportError(NklState nkl, NklToken const *token, char const *fmt, va_list ap) {
    if (!token) {
        static NklToken const dummy_token = {0};
        token = &dummy_token;
    }

    NklError *node = nk_arena_allocT(&nkl->permanent_arena, NklError);
    *node = (NklError){
        .next = NULL,
        .msg = {0},
        .token = token,
    };

    if (!nkl->errors.errors) {
        nkl->errors.errors = nkl->errors.last_error = node;
    } else {
        nkl->errors.last_error->next = node;
        nkl->errors.last_error = node;
    }
    nkl->errors.error_count++;

    NkStringBuilder sb = (NkStringBuilder){NKSB_INIT(nk_arena_getAllocator(&nkl->permanent_arena))};
    i32 res = nksb_vprintf(&sb, fmt, ap);
    nk_arena_pop(&nkl->permanent_arena, sb.capacity - sb.size);
    node->msg = (NkString){NKS_INIT(sb)};

    return res;
}
