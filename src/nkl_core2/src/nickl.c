#include "nkl/core/nickl.h"

#include "ast_tokens.h"
#include "nkb/ir.h"
#include "nkb/types.h"
#include "nkl/common/ast.h"
#include "nkl/common/diagnostics.h"
#include "nkl/common/token.h"
#include "nkl/core/ast_parser.h"
#include "nkl/core/ir_parser.h"
#include "nkl/core/lexer.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/error.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/slice.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(nickl);

typedef struct NklState_T {
    struct NklState_T *next;

    NkArena arena;
    NklError *error;
} NklState_T;

typedef struct NklCompiler_T {
    NklState nkl;
} NklCompiler_T;

typedef struct NklModule_T {
    NklCompiler c;
    NkIrSymbolDynArray ir;
} NklModule_T;

static void vreportError(NklState nkl, NklSourceLocation loc, char const *fmt, va_list ap) {
    NklError *err = nk_arena_allocT(&nkl->arena, NklError);
    *err = (NklError){
        .next = nkl->error,
        .msg = nk_vtsprintf(&nkl->arena, fmt, ap),
        .loc = loc,
    };
    nkl->error = err;
}

NK_PRINTF_LIKE(3) static void reportError(NklState nkl, NklSourceLocation loc, char const *fmt, ...) {
    NK_LOG_TRC("%s", __func__);

    va_list ap;
    va_start(ap, fmt);
    vreportError(nkl, loc, fmt, ap);
    va_end(ap);
}

NklState nkl_newState(void) {
    NK_LOG_TRC("%s", __func__);

    NkArena arena = {0};
    NklState nkl = nk_arena_allocT(&arena, NklState_T);
    *nkl = (NklState_T){
        .arena = arena,
        .error = NULL,
    };
    return nkl;
}

void nkl_freeState(NklState nkl) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(nkl && "state is null");

    NkArena arena = nkl->arena;
    nk_arena_free(&arena);
}

static _Thread_local NklState s_nkl;

void nkl_pushState(NklState nkl) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(nkl && "state is null");

    nkl->next = s_nkl;
    s_nkl = nkl;
}

void nkl_popState(void) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(s_nkl && "no active state");
    s_nkl = s_nkl->next;
}

NklCompiler nkl_newCompiler(NklTargetTriple target) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(s_nkl && "no active state");
    NklState nkl = s_nkl;

    (void)target; // TODO: Use target triple
    NklCompiler c = nk_arena_allocT(&nkl->arena, NklCompiler_T);
    *c = (NklCompiler_T){
        .nkl = nkl,
    };
    return c;
}

NklCompiler nkl_newCompilerHost(void) {
    NK_LOG_TRC("%s", __func__);

    // TODO: Actually fill host target triple
    return nkl_newCompiler((NklTargetTriple){0});
}

NklModule nkl_newModule(NklCompiler c) {
    NK_LOG_TRC("%s", __func__);

    if (!c) {
        return NULL;
    }

    NklState nkl = c->nkl;

    NklModule mod = nk_arena_allocT(&nkl->arena, NklModule_T);
    *mod = (NklModule_T){
        .c = c,
        .ir = {NKDA_INIT(nk_arena_getAllocator(&nkl->arena))},
    };
    return mod;
}

bool nkl_linkModule(NklModule dst_mod, NklModule src_mod) {
    NK_LOG_TRC("%s", __func__);

    if (!dst_mod) {
        return false;
    }

    NklState nkl = dst_mod->c->nkl;

    if (!src_mod) {
        reportError(nkl, (NklSourceLocation){0}, "src_mod is null");
        return false;
    }

    if (dst_mod->c != src_mod->c) {
        reportError(nkl, (NklSourceLocation){0}, "mixed modules from different compilers");
    }

    reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_linkModule` is not implemented");
    return false;
}

bool nkl_linkLibrary(NklModule dst_mod, NkString name, NkString library) {
    NK_LOG_TRC("%s", __func__);

    if (!dst_mod) {
        return false;
    }

    NklState nkl = dst_mod->c->nkl;

    (void)name;
    (void)library;
    reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_linkLibrary` is not implemented");
    return false;
}

bool nkl_compileFile(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    NkString const ext = nk_path_getExtension(file);

    if (nks_equal(ext, nk_cs2s("nkir"))) {
        return nkl_compileFileIr(mod, file);
    } else if (nks_equal(ext, nk_cs2s("nkst"))) {
        return nkl_compileFileAst(mod, file);
    } else if (nks_equal(ext, nk_cs2s("nkl"))) {
        return nkl_compileFileNkl(mod, file);
    } else {
        reportError(
            nkl,
            (NklSourceLocation){0},
            "Unsupported source file `*." NKS_FMT "`. Supported: `*.nkir`, `*.nkst`, `*.nkl`.",
            NKS_ARG(ext));
        return false;
    }
}

char const *s_ir_keywords[] = {
    "pub",
    "proc",
    "i32",
    "ret",

    NULL,
};

char const *s_ir_operators[] = {
    "(",
    ")",
    "{",
    "}",

    NULL,
};

char const s_ir_tag_prefixes[] = {
    '@',

    0,
};

enum {
    NklIrToken_KeywordsBase = NklToken_Count,

    NklIrToken_Pub,
    NklIrToken_Proc,
    NklIrToken_I32,
    NklIrToken_Ret,

    NklIrToken_OperatorsBase,

    NklIrToken_LParen,
    NklIrToken_RParen,
    NklIrToken_LBrace,
    NklIrToken_RBrace,

    NklIrToken_TagsBase,

    NklIrToken_AtTag,
};

char const *s_ir_token_names[] = {
    "end of file", // NklToken_Eof

    "identifier",      // NklToken_Id
    "int constant",    // NklToken_Int
    "float constant",  // NklToken_Float
    "string constant", // NklToken_String
    "string constant", // NklToken_EscapedString

    "error", // NklToken_Error

    NULL, // NklIrToken_KeywordsBase

    "pub",  // NklIrToken_Pub
    "proc", // NklIrToken_Proc
    "i32",  // NklIrToken_I32
    "ret",  // NklIrToken_Ret

    NULL, // NklIrToken_OperatorsBase

    "(", // NklIrToken_LParen
    ")", // NklIrToken_RParen
    "{", // NklIrToken_LBrace
    "}", // NklIrToken_RBrace

    NULL, // NklIrToken_TagsBase

    "@", // NklIrToken_AtTag
};

bool nkl_compileFileIr(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    NkAllocator alloc = nk_arena_getAllocator(&nkl->arena);

    NkString text;
    if (!nk_file_read(alloc, file, &text)) {
        reportError(
            nkl,
            (NklSourceLocation){0},
            "failed to read file `" NKS_FMT "`: %s",
            NKS_ARG(file),
            nk_getLastErrorString());
        return false;
    }

    NkString err_str;
    NklTokenArray tokens;
    if (!nkl_lex(
            &(NklLexerData){
                .text = text,
                .arena = &nkl->arena,
                .err_str = &err_str,

                .keywords = s_ir_keywords,
                .operators = s_ir_operators,
                .tag_prefixes = s_ir_tag_prefixes,

                .keywords_base = NklIrToken_KeywordsBase,
                .operators_base = NklIrToken_OperatorsBase,
                .tags_base = NklIrToken_TagsBase,
            },
            &tokens)) {
        NklToken const err_token = nk_slice_last(tokens);
        reportError(
            nkl,
            (NklSourceLocation){
                .file = file,
                .lin = err_token.lin,
                .col = err_token.col,
                .len = err_token.len,
            },
            NKS_FMT,
            NKS_ARG(err_str));
        return false;
    }

#if 0
    { // TODO: Dummy IR
        NkIrType_T const void_t = {.aggr = {0}, .size = 0, .align = 1, .id = 0, .kind = NkIrType_Aggregate};

        NkIrType_T const i8_t = {.num = Int8, .size = 1, .align = 1, .id = 0, .kind = NkIrType_Numeric};
        NkIrType_T const i32_t = {.num = Int32, .size = 4, .align = 4, .id = 0, .kind = NkIrType_Numeric};
        NkIrType_T const i64_t = {.num = Int64, .size = 8, .align = 8, .id = 0, .kind = NkIrType_Numeric};
        NkIrType_T const f64_t = {.num = Float64, .size = 8, .align = 8, .id = 0, .kind = NkIrType_Numeric};
        NkIrType_T const ptr_t = i64_t;

        NkAtom l_start = nk_atom_unique(nk_cs2s("start"));
        NkAtom l_loop = nk_atom_unique(nk_cs2s("loop"));
        NkAtom l_endloop = nk_atom_unique(nk_cs2s("endloop"));

        NkAtom s_hello = nk_atom_unique(nk_cs2s("hello"));
        NkAtom s_fmt = nk_atom_unique(nk_cs2s("fmt"));

        NkIrAggregateElemInfo const hello_str_elems[] = {
            {.type = &i8_t, .count = 15, .offset = 0},
        };
        NkIrType_T const hello_str_t = {
            .aggr = {hello_str_elems, NK_ARRAY_COUNT(hello_str_elems)},
            .size = 15,
            .align = 1,
            .id = 0,
            .kind = NkIrType_Aggregate,
        };
        nkda_append(
            &mod->ir,
            ((NkIrSymbol){
                .data =
                    {
                        .type = &hello_str_t,
                        .relocs = {0},
                        .addr = "Hello, World!\n",
                        .flags = NkIrData_ReadOnly,
                    },
                .name = s_hello,
                .vis = NkIrVisibility_Local,
                .flags = 0,
                .kind = NkIrSymbol_Data,
            }));

        NkIrInstrDynArray main_instrs = {NKDA_INIT(alloc)};

        // @start
        nkda_append(&main_instrs, nkir_make_label(l_start));

        // call printf, ("Hello, World!", ...)
        NkIrRef const printf_args[] = {
            nkir_makeRefGlobal(s_hello, &ptr_t),
            nkir_makeVariadicMarker(),
        };
        nkda_append(
            &main_instrs,
            nkir_make_call(
                nkir_makeRefNull(&i32_t),
                nkir_makeRefGlobal(nk_cs2atom("printf"), &ptr_t),
                (NkIrRefArray){printf_args, NK_ARRAY_COUNT(printf_args)}));

        // // This is a comment...
        nkda_append(&main_instrs, nkir_make_comment(nk_cs2s("This is a comment...")));

        // ret 0
        nkda_append(&main_instrs, nkir_make_ret(nkir_makeRefImm((NkIrImm){.i32 = 0}, &i32_t)));

        nkda_append(
            &mod->ir,
            ((NkIrSymbol){
                .proc =
                    {
                        .params = {0},
                        .ret_type = &i32_t,
                        .instrs = {NK_SLICE_INIT(main_instrs)},
                        .file = {0},
                        .line = 0,
                        .flags = 0,
                    },
                .name = nk_cs2atom("main"),
                .vis = NkIrVisibility_Default,
                .flags = 0,
                .kind = NkIrSymbol_Proc,
            }));

        NkIrInstrDynArray plus_instrs = {NKDA_INIT(alloc)};

        // @start
        nkda_append(&plus_instrs, nkir_make_label(l_start));

        // add a, b -> tmp:i64
        nkda_append(
            &plus_instrs,
            nkir_make_add(
                nkir_makeRefLocal(nk_cs2atom("tmp"), &i64_t),
                nkir_makeRefParam(nk_cs2atom("a"), &i64_t),
                nkir_makeRefParam(nk_cs2atom("b"), &i64_t)));

        // ret tmp:i64
        nkda_append(&plus_instrs, nkir_make_ret(nkir_makeRefLocal(nk_cs2atom("tmp"), &i64_t)));

        NkIrParam plus_params[] = {
            {.name = nk_cs2atom("a"), .type = &i64_t},
            {.name = nk_cs2atom("b"), .type = &i64_t},
        };
        nkda_append(
            &mod->ir,
            ((NkIrSymbol){
                .proc =
                    {
                        .params = {plus_params, NK_ARRAY_COUNT(plus_params)},
                        .ret_type = &i64_t,
                        .instrs = {NK_SLICE_INIT(plus_instrs)},
                        .file = {0},
                        .line = 0,
                        .flags = 0,
                    },
                .name = nk_cs2atom("plus"),
                .vis = NkIrVisibility_Default,
                .flags = 0,
                .kind = NkIrSymbol_Proc,
            }));

        NkIrAggregateElemInfo const fmt_str_elems[] = {
            {.type = &i8_t, .count = 5, .offset = 0},
        };
        NkIrType_T const fmt_str_t = {
            .aggr = {fmt_str_elems, NK_ARRAY_COUNT(fmt_str_elems)},
            .size = 5,
            .align = 1,
            .id = 0,
            .kind = NkIrType_Aggregate,
        };
        nkda_append(
            &mod->ir,
            ((NkIrSymbol){
                .data =
                    {
                        .type = &fmt_str_t,
                        .relocs = {0},
                        .addr = "%zi\n",
                        .flags = NkIrData_ReadOnly,
                    },
                .name = s_fmt,
                .vis = NkIrVisibility_Local,
                .flags = 0,
                .kind = NkIrSymbol_Data,
            }));

        NkIrInstrDynArray loop_instrs = {NKDA_INIT(alloc)};

        // @loop
        nkda_append(&loop_instrs, nkir_make_label(l_loop));

        // cmp lt i, 5 -> cond
        nkda_append(
            &loop_instrs,
            nkir_make_cmp_lt(
                nkir_makeRefLocal(nk_cs2atom("cond"), &i8_t),
                nkir_makeRefLocal(nk_cs2atom("i"), &i64_t),
                nkir_makeRefImm((NkIrImm){.i64 = 5}, &i64_t)));

        // jmpz cond, @endloop
        nkda_append(&loop_instrs, nkir_make_jmpz(nkir_makeRefLocal(nk_cs2atom("cond"), &i8_t), l_endloop));

        // call printf, ("%zi\n", ..., i)
        NkIrRef const printf_args2[] = {
            nkir_makeRefGlobal(s_fmt, &ptr_t),
            nkir_makeVariadicMarker(),
            nkir_makeRefLocal(nk_cs2atom("i"), &i64_t),
        };
        nkda_append(
            &loop_instrs,
            nkir_make_call(
                nkir_makeRefNull(&i32_t),
                nkir_makeRefGlobal(nk_cs2atom("printf"), &ptr_t),
                (NkIrRefArray){printf_args2, NK_ARRAY_COUNT(printf_args2)}));

        // // This is another comment...
        nkda_append(&loop_instrs, nkir_make_comment(nk_cs2s("This is another comment...")));

        // add i, 1 -> i
        nkda_append(
            &loop_instrs,
            nkir_make_add(
                nkir_makeRefLocal(nk_cs2atom("i"), &i64_t),
                nkir_makeRefLocal(nk_cs2atom("i"), &i64_t),
                nkir_makeRefImm((NkIrImm){.i64 = 1}, &i64_t)));

        // jmp @loop
        nkda_append(&loop_instrs, nkir_make_jmp(l_loop));

        // @endloop
        nkda_append(&loop_instrs, nkir_make_label(l_endloop));

        // ret
        nkda_append(&loop_instrs, nkir_make_ret((NkIrRef){0}));

        NkIrInstrDynArray loop_instrs_pic = {NKDA_INIT(alloc)};
        nkir_convertToPic((NkIrInstrArray){NK_SLICE_INIT(loop_instrs)}, &loop_instrs_pic);

        nkda_append(
            &mod->ir,
            ((NkIrSymbol){
                .proc =
                    {
                        .params = {0},
                        .ret_type = &void_t,
                        .instrs = {NK_SLICE_INIT(loop_instrs_pic)},
                        .file = {0},
                        .line = 0,
                        .flags = 0,
                    },
                .name = l_loop,
                .vis = NkIrVisibility_Default,
                .flags = 0,
                .kind = NkIrSymbol_Proc,
            }));

        NkIrAggregateElemInfo const vec2_elems[] = {
            {.type = &f64_t, .count = 2, .offset = 0},
        };
        NkIrType_T const vec2_t = {
            .aggr = {vec2_elems, NK_ARRAY_COUNT(vec2_elems)},
            .size = 16,
            .align = 1,
            .id = 0,
            .kind = NkIrType_Aggregate,
        };

        NkIrInstrDynArray makeVec2_instrs = {NKDA_INIT(alloc)};

        // @start
        nkda_append(&makeVec2_instrs, nkir_make_label(l_start));

        // alloc :vec2 -> %vec:i64
        nkda_append(&makeVec2_instrs, nkir_make_alloc(nkir_makeRefLocal(nk_cs2atom("vec"), &ptr_t), &vec2_t));

        // store %vec, %x
        nkda_append(
            &makeVec2_instrs,
            nkir_make_store(nkir_makeRefLocal(nk_cs2atom("vec"), &ptr_t), nkir_makeRefParam(nk_cs2atom("x"), &f64_t)));

        // add %vec:i64, 8:i64 -> %y_addr:i64
        nkda_append(
            &makeVec2_instrs,
            nkir_make_add(
                nkir_makeRefLocal(nk_cs2atom("y_addr"), &ptr_t),
                nkir_makeRefLocal(nk_cs2atom("vec"), &ptr_t),
                nkir_makeRefImm((NkIrImm){.i64 = 8}, &i64_t)));

        // store %vec, %x
        nkda_append(
            &makeVec2_instrs,
            nkir_make_store(
                nkir_makeRefLocal(nk_cs2atom("y_addr"), &ptr_t), nkir_makeRefParam(nk_cs2atom("y"), &f64_t)));

        // ret
        nkda_append(&makeVec2_instrs, nkir_make_ret(nkir_makeRefLocal(nk_cs2atom("vec"), &ptr_t)));

        NkIrParam makeVec2_params[] = {
            {.name = nk_cs2atom("x"), .type = &f64_t},
            {.name = nk_cs2atom("y"), .type = &f64_t},
        };
        nkda_append(
            &mod->ir,
            ((NkIrSymbol){
                .proc =
                    {
                        .params = {makeVec2_params, NK_ARRAY_COUNT(makeVec2_params)},
                        .ret_type = &vec2_t,
                        .instrs = {NK_SLICE_INIT(makeVec2_instrs)},
                        .file = {0},
                        .line = 0,
                        .flags = 0,
                    },
                .name = nk_cs2atom("makeVec2"),
                .vis = NkIrVisibility_Default,
                .flags = 0,
                .kind = NkIrSymbol_Proc,
            }));

        NkStringBuilder sb = {NKSB_INIT(alloc)};
        nkir_inspectModule((NkIrModule){NK_SLICE_INIT(mod->ir)}, nksb_getStream(&sb));
        NK_LOG_INF("IR:\n" NKS_FMT, NKS_ARG(sb));
    }
#endif

    NklToken err_token = {0};
    if (!nkl_ir_parse(
            &(NklIrParserData){
                .text = text,
                .tokens = tokens,
                .arena = &nkl->arena,
                .err_str = &err_str,
                .err_token = &err_token,
                .token_names = s_ir_token_names,
            },
            &mod->ir)) {
        reportError(
            nkl,
            (NklSourceLocation){
                .file = file,
                .lin = err_token.lin,
                .col = err_token.col,
                .len = err_token.len,
            },
            NKS_FMT,
            NKS_ARG(err_str));
        return false;
    }

    reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_compileFileIr` is not finished");
    return false;
}

char const *s_ast_operators[] = {
    "(",
    ")",
    "{",
    "}",
    "[",
    "]",

    NULL,
};

char const *s_ast_token_names[] = {
    "end of file", // NklToken_Eof

    "identifier",      // NklToken_Id
    "int constant",    // NklToken_Int
    "float constant",  // NklToken_Float
    "string constant", // NklToken_String
    "string constant", // NklToken_EscapedString

    "error", // NklToken_Error

    NULL, // NklAstToken_OperatorsBase

    "(", // NklAstToken_LParen
    ")", // NklAstToken_RParen
    "{", // NklAstToken_LBrace
    "}", // NklAstToken_RBrace
    "[", // NklAstToken_LBraket
    "]", // NklAstToken_RBraket
};

bool nkl_compileFileAst(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    NkAllocator alloc = nk_arena_getAllocator(&nkl->arena);

    NkString text = {0};
    if (!nk_file_read(alloc, file, &text)) {
        reportError(
            nkl,
            (NklSourceLocation){0},
            "failed to read file `" NKS_FMT "`: %s",
            NKS_ARG(file),
            nk_getLastErrorString());
        return false;
    }

    NkString err_str = {0};
    NklTokenArray tokens = {0};
    if (!nkl_lex(
            &(NklLexerData){
                .text = text,
                .arena = &nkl->arena,
                .err_str = &err_str,

                .operators = s_ast_operators,
                .operators_base = NklAstToken_OperatorsBase,
            },
            &tokens)) {
        NklToken const err_token = nk_slice_last(tokens);
        reportError(
            nkl,
            (NklSourceLocation){
                .file = file,
                .lin = err_token.lin,
                .col = err_token.col,
                .len = err_token.len,
            },
            NKS_FMT,
            NKS_ARG(err_str));
        return false;
    }

    NklToken err_token = {0};
    NklAstNodeArray nodes = {0};
    if (!nkl_ast_parse(
            &(NklAstParserData){
                .text = text,
                .tokens = tokens,
                .arena = &nkl->arena,
                .err_str = &err_str,
                .err_token = &err_token,
                .token_names = s_ast_token_names,
            },
            &nodes)) {
        reportError(
            nkl,
            (NklSourceLocation){
                .file = file,
                .lin = err_token.lin,
                .col = err_token.col,
                .len = err_token.len,
            },
            NKS_FMT,
            NKS_ARG(err_str));
        return false;
    }

    reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_compileFileAst` is not finished");
    return false;
}

bool nkl_compileFileNkl(NklModule mod, NkString file) {
    NK_LOG_TRC("%s", __func__);

    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    (void)file;
    reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_compileFileNkl` is not implemented");
    return false;
}

bool nkl_exportModule(NklModule mod, NkString out_file, NklOutputKind kind) {
    NK_LOG_TRC("%s", __func__);

    if (!mod) {
        return false;
    }

    NklState nkl = mod->c->nkl;

    (void)out_file;
    (void)kind;
    reportError(nkl, (NklSourceLocation){0}, "TODO: `nkl_exportModule` is not implemented");
    return false;
}

NklError const *nkl_getErrors(NklState nkl) {
    return nkl->error;
}
