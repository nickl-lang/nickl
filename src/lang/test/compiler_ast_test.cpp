#include <algorithm>
#include <iterator>

#include <gtest/gtest.h>

#include "nkl/lang/ast.h"
#include "nkl/lang/compiler.h"
#include "nkl/lang/value.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/string.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(test);

class compiler_ast : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});

        char path_buf[NK_MAX_PATH];
        int path_len = nk_getBinaryPath(path_buf, sizeof(path_buf));
        if (path_len < 0) {
            FAIL() << "failed to get the compiler binary path";
        }

        m_ast = nkl_ast_create();
        m_compiler = nkl_compiler_create();
        nkl_compiler_configure(m_compiler, {path_buf, (usize)path_len});
    }

    void TearDown() override {
        nk_arena_free(&m_arena);
        nkl_compiler_free(m_compiler);
        nkl_ast_free(m_ast);

        nkl_types_clean();
    }

protected:
    void test(NklAstNodeArray root) {
#ifdef ENABLE_LOGGING
        {
            NkStringBuilder sb{};
            defer {
                nksb_free(&sb);
            };
            nkl_inspectNode(root.data, &sb);
            NK_LOG_INF("ast:" NKS_FMT "\n", NKS_ARG(sb));
        }
#endif // ENABLE_LOGGING
        nkl_compiler_run(m_compiler, root.data);
    }

    NklToken const *mkt(char const *text) {
        return new (nk_arena_allocT<NklToken>(&m_arena)) NklToken{
            .text = text ? nk_cs2s(text) : NkString{},
            .pos = 0,
            .lin = 0,
            .col = 0,
            .id = 0,
        };
    }

    NklAstNodeArray _(char const *id, char const *text) {
        return _(id, text, {}, {}, {});
    }

    NklAstNodeArray _(char const *id, NklAstNodeArray arg0) {
        return _(id, arg0, {}, {});
    }

    NklAstNodeArray _(char const *id, NklAstNodeArray arg0, NklAstNodeArray arg1) {
        return _(id, arg0, arg1, {});
    }

    NklAstNodeArray _(char const *id, NklAstNodeArray arg0, NklAstNodeArray arg1, NklAstNodeArray arg2) {
        return _(id, {}, arg0, arg1, arg2);
    }

    NklAstNodeArray _(
        char const *id,
        char const *text,
        NklAstNodeArray arg0,
        NklAstNodeArray arg1,
        NklAstNodeArray arg2) {
        return nkl_pushNode(
            m_ast,
            {
                .args{arg0, arg1, arg2},
                .token = mkt(text),
                .id = nk_cs2atom(id),
            });
    }

    NklAstNodeArray _(std::vector<NklAstNodeArray> const &ar) {
        std::vector<NklAstNode_T> nodes;
        std::transform(ar.begin(), ar.end(), std::back_inserter(nodes), [](NklAstNodeArray ar) {
            return ar.data[0];
        });
        return nkl_pushNodeAr(m_ast, {nodes.data(), nodes.size()});
    }

protected:
    NklAst m_ast;
    NklCompiler m_compiler;
    NkArena m_arena{};
};

} // namespace

TEST_F(compiler_ast, empty) {
    test({});
}

TEST_F(compiler_ast, basic) {
    test(_("add", _("int", "2"), _("int", "2")));
}

TEST_F(compiler_ast, comptime_const) {
    test(_("comptime_const_def", _("id", "pi"), _("float", "3.14")));
}

TEST_F(compiler_ast, fn) {
    test(
        _("block",
          _({
              _("comptime_const_def",
                _("id", "add"),
                _("fn",
                  _({
                      _("param", _("id", "lhs"), _("i64", "i64")),
                      _("param", _("id", "rhs"), _("i64", "i64")),
                  }),
                  _("i64", "i64"),
                  _("block", _("return", _("add", _("id", "lhs"), _("id", "rhs")))))),
              _("call",
                _("id", "add"),
                _({
                    _("arg", {}, _("int", "4")),
                    _("arg", {}, _("int", "5")),
                })),
          })));
}

TEST_F(compiler_ast, comptime_const_getter) {
    test(
        _("block",
          _({
              _("define", _("id", "counter"), _("int", "0")),
              _("comptime_const_def",
                _("id", "getVal"),
                _("fn",
                  _({}),
                  _("i64", "i64"),
                  _("block",
                    _({
                        _("assign", _("id", "counter"), _("add", _("id", "counter"), _("int", "1"))),
                        _("return", _("int", "42")),
                    })))),
              _("comptime_const_def", _("id", "val"), _("call", _("id", "getVal"), _({}))),
              _("id", "val"),
              _("id", "val"),
          })));
}

TEST_F(compiler_ast, native_puts) {
    test(
        _("block",
          _({
              _("tag",
                _("name", "#link"),
                _("arg", {}, _("string", R"("")")),
                _("comptime_const_def",
                  _("id", "puts"),
                  _("fn_type", _("param", _("id", "str"), _("ptr_type", _("i8", "i8"))), _("void", "void")))),
              _("call", _("id", "puts"), _("arg", {}, _("string", R"("Hello, World!")"))),
          })));
}

TEST_F(compiler_ast, fast_exp) {
    test(
        _("block",
          _({
              _("define", _("id", "b"), _("int", "2")),
              _("define", _("id", "n"), _("int", "16")),
              _("define", _("id", "a"), _("int", "1")),
              _("define", _("id", "c"), _("id", "b")),
              _("while",
                _("ne", _("id", "n"), _("int", "0")),
                _("block",
                  _({
                      _("define", _("id", "r"), _("mod", _("id", "n"), _("int", "2"))),
                      _("if",
                        _("eq", _("id", "r"), _("int", "1")),
                        _("assign", _("id", "a"), _("mul", _("id", "a"), _("id", "c")))),
                      _("assign", _("id", "n"), _("div", _("id", "n"), _("int", "2"))),
                      _("assign", _("id", "c"), _("mul", _("id", "c"), _("id", "c"))),
                  }))),
          })));
}

TEST_F(compiler_ast, import) {
    test(
        _("block",
          _({
              _("import", _("id", "libc")),
              _("call", _("member", _("id", "libc"), _("id", "puts")), _("arg", {}, _("string", R"("Hello, World!")"))),
          })));
}

TEST_F(compiler_ast, comptime_declareLocal) {
    test(
        _("block",
          _({
              _("import", _("id", "libc")),
              _("import", _("id", "compiler")),
              _("run",
                _("call",
                  _("member", _("id", "compiler"), _("id", "declareLocal")),
                  _({
                      _("arg", {}, _("string", R"("str")")),
                      _("arg", {}, _("ptr_type", _("i8", "i8"))),
                  }))),
              _("assign", _("id", "str"), _("string", R"("hello")")),
              _("call", _("member", _("id", "libc"), _("id", "puts")), _("arg", {}, _("id", "str"))),
          })));
}
