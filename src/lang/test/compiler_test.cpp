#include "nkl/lang/compiler.h"

#include <algorithm>
#include <iterator>
#include <new>

#include <gtest/gtest.h>

#include "nk/common/allocator.h"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/utils.hpp"
#include "nkl/lang/ast.h"

namespace {

NK_LOG_USE_SCOPE(test);

class compiler : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT(NkLoggerOptions{});

        m_ast = nkl_ast_create();
        m_compiler = nkl_compiler_create({}); // TODO Providing empty stdlib dir
        m_arena = nk_create_arena();
    }

    void TearDown() override {
        nk_free_arena(m_arena);
        nkl_compiler_free(m_compiler);
        nkl_ast_free(m_ast);
    }

protected:
    void inspect(NklAstNodeArray node) {
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };

        nkl_ast_inspect(node.data, sb);
        auto str = nksb_concat(sb);

        NK_LOG_INF("ast:%.*s\n", str.size, str.data);
    }

    NklToken const *mkt(char const *text) {
        return new (nk_allocate(m_arena, sizeof(NklToken))) NklToken{
            .text = text ? cs2s(text) : nkstr{},
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

    NklAstNodeArray _(
        char const *id,
        NklAstNodeArray arg0,
        NklAstNodeArray arg1,
        NklAstNodeArray arg2) {
        return _(id, {}, arg0, arg1, arg2);
    }

    NklAstNodeArray _(
        char const *id,
        char const *text,
        NklAstNodeArray arg0,
        NklAstNodeArray arg1,
        NklAstNodeArray arg2) {
        NklAstNode_T node{
            .args{arg0, arg1, arg2},
            .token = mkt(text),
            .id = cs2nkid(id),
        };
        return {nkl_ast_pushNode(m_ast, &node), 1};
    }

    NklAstNodeArray _(std::vector<NklAstNodeArray> const &ar) {
        std::vector<NklAstNode_T> nodes;
        std::transform(ar.begin(), ar.end(), std::back_inserter(nodes), [](NklAstNodeArray ar) {
            return ar.data[0];
        });
        return nkl_ast_pushNodeAr(m_ast, {nodes.data(), nodes.size()});
    }

protected:
    NklAst m_ast;
    NklCompiler m_compiler;
    NkAllocator *m_arena;
};

} // namespace

TEST_F(compiler, empty) {
    auto n_empty = NklAstNodeArray{};
    inspect(n_empty);
    nkl_compiler_run(m_compiler, n_empty.data);
}

TEST_F(compiler, basic) {
    auto n_root = _("add", _("int", "2"), _("int", "2"));
    inspect(n_root);
    nkl_compiler_run(m_compiler, n_root.data);
}

TEST_F(compiler, comptime_const) {
    auto n_root = _("comptime_const_def", _("id", "pi"), _("float", "3.14"));
    inspect(n_root);
    nkl_compiler_run(m_compiler, n_root.data);
}

TEST_F(compiler, comptime_const_getter) {
    auto n_root = _(
        "block",
        _({
            _("define", _("id", "counter"), _("int", "0")),
            _("comptime_const_def",
              _("id", "getVal"),
              _("fn",
                _({}),
                _("u32", "u32"),
                _("block",
                  _({
                      _("assign", _("id", "counter"), _("add", _("id", "counter"), _("int", "1"))),
                      _("return", _("int", "42")),
                  })))),
            _("comptime_const_def", _("id", "val"), _("call", _("id", "getVal"), _({}))),
            _("id", "val"),
            _("id", "val"),
        }));
    inspect(n_root);
    nkl_compiler_run(m_compiler, n_root.data);
}

TEST_F(compiler, fn) {
    auto n_root =
        _("block",
          _({
              _("comptime_const_def",
                _("id", "add"),
                _("fn",
                  _({
                      _("param", _("id", "lhs"), _("u32", "u32")),
                      _("param", _("id", "rhs"), _("u32", "u32")),
                  }),
                  _("u32", "u32"),
                  _("block", _("return", _("add", _("id", "lhs"), _("id", "rhs")))))),
              _("call",
                _("id", "add"),
                _({
                    _("int", "4"),
                    _("int", "5"),
                })),
          }));

    inspect(n_root);

    nkl_compiler_run(m_compiler, n_root.data);
}

TEST_F(compiler, native_puts) {
    auto n_root =
        _("block",
          _({
              _("tag",
                _("name", "#foreign"),
                _("string", ""),
                _("comptime_const_def",
                  _("id", "puts"),
                  _("fn_type",
                    _("param", _("id", "str"), _("ptr_type", _("u8", "u8"))),
                    _("void", "void")))),
              _("call", _("id", "puts"), _("string", "Hello, World!")),
          }));

    inspect(n_root);

    nkl_compiler_run(m_compiler, n_root.data);
}

TEST_F(compiler, fast_exp) {
    auto n_root =
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
          }));

    inspect(n_root);

    nkl_compiler_run(m_compiler, n_root.data);
}

TEST_F(compiler, import) {
    auto n_root =
        _("block",
          _({
              _("import", _("id", "std")),
              _("call", _("member", _("id", "std"), _("id", "puts")), _("string", "Hello, World!")),
          }));

    inspect(n_root);

    nkl_compiler_run(m_compiler, n_root.data);
}

TEST_F(compiler, comptime_declareLocal) {
    auto n_root =
        _("block",
          _({
              _("import", _("id", "std")),
              _("import", _("id", "compiler")),
              _("run",
                _("call",
                  _("member", _("id", "compiler"), _("id", "declareLocal")),
                  _({_("string", "a"), _("i64", "i64")}))),
              _("assign", _("id", "a"), _("int", "65")),
              _("call", _("member", _("id", "std"), _("id", "putchar")), _("id", "a")),
          }));

    inspect(n_root);

    nkl_compiler_run(m_compiler, n_root.data);
}
