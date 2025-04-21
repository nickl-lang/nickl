#include <filesystem>

#include "irc_impl.hpp"
#include "lexer.h"
#include "nkb/ir.h"
#include "nkl/common/diagnostics.h"
#include "nkl/core/search.h"
#include "ntk/allocator.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dl.h"
#include "ntk/dyn_array.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"
#include "parser.h"

namespace {

namespace fs = std::filesystem;

NK_LOG_USE_SCOPE(nkirc);

} // namespace

NkIrCompiler nkirc_create(NkArena *tmp_arena, NkIrcConfig conf) {
    NkIrCompiler c = new (nk_allocT<NkIrCompiler_T>(nk_default_allocator)) NkIrCompiler_T{
        .tmp_arena = tmp_arena,
        .conf = conf,
    };
    c->parser.decls = decltype(NkIrParserState::decls)::create(nk_arena_getAllocator(&c->parse_arena));
    c->extern_sym = {NKDA_INIT(nk_arena_getAllocator(&c->parse_arena))};
    return c;
}

void nkirc_free(NkIrCompiler c) {
    c->fpmap.deinit();

    nk_arena_free(&c->parse_arena);
    nk_arena_free(&c->file_arena);

    nk_freeT(nk_default_allocator, c);
}

int nkir_compile(NkIrCompiler c, NkString in_file, NkIrCompilerConfig conf) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    c->ir = nkir_createProgram(&c->file_arena);
    c->mod = nkir_createModule(c->ir);

    auto base_path_str = (fs::current_path() / "_").string();
    NkString base_file{base_path_str.c_str(), base_path_str.size()};

    if (!nkir_compileFile(c, base_file, in_file)) {
        return 1;
    }

    if (conf.output_kind == NkbOutput_Executable && c->entry_point.idx == NKIR_INVALID_IDX) {
        nkl_diag_printError("entry point is not defined");
        return 1;
    }

    if (!nkir_write(c->ir, c->mod, c->tmp_arena, conf)) {
        NkString err_str = nkir_getErrorString(c->ir);
        nkl_diag_printError("failed to write output: " NKS_FMT, NKS_ARG(err_str));
        return 1;
    }

    return 0;
}

int nkir_run(NkIrCompiler c, NkString in_file) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    c->ir = nkir_createProgram(&c->file_arena);
    c->mod = nkir_createModule(c->ir);

    auto base_path_str = (fs::current_path() / "_").string();
    NkString base_file{base_path_str.c_str(), base_path_str.size()};
    if (!nkir_compileFile(c, base_file, in_file)) {
        return 1;
    }

    if (c->entry_point.idx == NKIR_INVALID_IDX) {
        nkl_diag_printError("entry point is not defined");
        return 1;
    }

    auto run_ctx = nkir_createRunCtx(c->ir, c->tmp_arena);
    defer {
        nkir_freeRunCtx(run_ctx);
    };

    for (auto const &sym : nk_iterate(c->extern_sym)) {
        NK_LOG_DBG("Loading library `%s`", nk_atom2cs(sym.lib));
        auto const lib = nkl_findLibrary(sym.lib);
        if (nk_handleIsZero(lib)) {
            nkl_diag_printError("failed to load library `%s`: %s", nk_atom2cs(sym.lib), nkdl_getLastErrorString());
            return 1;
        }
        NK_LOG_DBG("Resolving symbol `%s`", nk_atom2cs(sym.sym));
        auto const addr = nkdl_resolveSymbol(lib, nk_atom2cs(sym.sym));
        NK_LOG_DBG("`%s` is @%p", nk_atom2cs(sym.sym), addr);
        nkir_setExternSymAddr(run_ctx, sym.sym, addr);
    }

    int argc = 1;
    char const *argv[] = {""};
    i64 ret_code = -1;

    void *args[] = {&argc, &argv};
    void *rets[] = {&ret_code};
    if (!nkir_invoke(run_ctx, c->entry_point, args, rets)) {
        NkString err_str = nkir_getRunErrorString(run_ctx);
        nkl_diag_printError("failed to run the program: " NKS_FMT, NKS_ARG(err_str));
        return 1;
    }

    return ret_code;
}

bool nkir_compileFile(NkIrCompiler c, NkString base_file, NkString in_file) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto in_file_path = fs::path{nk_s2stdStr(base_file)}.parent_path() / fs::path{nk_s2stdStr(in_file)};

    if (!fs::exists(in_file_path)) {
        auto const in_file_path_str = in_file_path.string();
        nkl_diag_printError("file `%s` doesn't exist", in_file_path_str.c_str());
        return false;
    }

    in_file_path = fs::canonical(in_file_path);
    auto const in_file_path_str = in_file_path.string();

    auto const in_file_s = NkString{in_file_path_str.c_str(), in_file_path_str.size()};

    NkString text;
    bool const ok = nk_file_read(nk_arena_getAllocator(&c->file_arena), in_file_s, &text);
    if (!ok) {
        nkl_diag_printError("failed to read file `%s`", in_file_path_str.c_str());
        return false;
    }

    auto const in_file_id = nk_s2atom(in_file_s);

    NkIrLexerState lexer{};
    {
        auto frame = nk_arena_grab(c->tmp_arena);
        defer {
            nk_arena_popFrame(c->tmp_arena, frame);
        };
        nkir_lex(&lexer, &c->file_arena, c->tmp_arena, text);
        if (!lexer.ok) {
            nkl_diag_printErrorQuote(
                text,
                {
                    in_file_s,
                    nk_slice_last(lexer.tokens).lin,
                    nk_slice_last(lexer.tokens).col,
                    nk_slice_last(lexer.tokens).len,
                },
                NKS_FMT,
                NKS_ARG(lexer.error_msg));
            return false;
        }
    }

    {
        auto frame = nk_arena_grab(c->tmp_arena);
        defer {
            nk_arena_popFrame(c->tmp_arena, frame);
        };
        nkir_parse(c, in_file_id, text, {NK_SLICE_INIT(lexer.tokens)});
        if (!c->parser.ok) {
            nkl_diag_printErrorQuote(
                text,
                {
                    in_file_s,
                    c->parser.error_token.lin,
                    c->parser.error_token.col,
                    c->parser.error_token.len,
                },
                NKS_FMT,
                NKS_ARG(c->parser.error_msg));
            return false;
        }
    }

#ifdef ENABLE_LOGGING
    NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(c->tmp_arena))};
    nkir_inspectProgram(c->ir, nksb_getStream(&sb));
    NK_LOG_INF("IR:\n" NKS_FMT, NKS_ARG(sb));
#endif // ENABLE_LOGGING

    return true;
}
