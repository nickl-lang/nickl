#include "nkl/core/lexer.hpp"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/string_builder.hpp"
#include "nk/vm/vm.hpp"

namespace {

using namespace nk;
using namespace nkl;

LOG_USE_SCOPE(nkl::lexer::test)

class lexer : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        vm::vm_init({});
    }

    void TearDown() override {
        m_lexer.tokens.deinit();

        vm::vm_deinit();
    }

protected:
    using IDVec = std::vector<ETokenID>;
    using StrVec = std::vector<std::string_view>;
    using TokenPos = std::array<size_t, 3>;
    using PosVec = std::vector<TokenPos>;

    void test_ok(const char *src) {
        LOG_INF("Test: %s", src)
        bool result = m_lexer.lex(cs2s(src));
        EXPECT_TRUE(result);
        if (!result) {
            reportError();
        }
    }

    void test_ok(const char *src, IDVec &&expected_id) {
        test_ok(src);
        expect_id(std::move(expected_id));
        expect_text({src});
    }

    void test_ok(const char *src, IDVec &&expected_id, StrVec &&expected_text) {
        test_ok(src);
        expect_id(std::move(expected_id));
        expect_text(std::move(expected_text));
    }

    void test_err(const char *src) {
        LOG_INF("Test: %s", src)
        bool result = m_lexer.lex(cs2s(src));
        EXPECT_FALSE(result);
        if (!result) {
            reportError();
        }
    }

    void expect_id(IDVec &&expected) {
        expected.emplace_back(t_eof);

        IDVec actual;
        actual.reserve(m_lexer.tokens.size);
        std::transform(
            std::begin(m_lexer.tokens),
            std::end(m_lexer.tokens),
            std::back_inserter(actual),
            [](const auto &token) -> ETokenID {
                return (ETokenID)token.id;
            });

        EXPECT_EQ(expected, actual);
    }

    void expect_text(StrVec &&expected) {
        expected.emplace_back("");

        StrVec actual;
        actual.reserve(m_lexer.tokens.size);
        std::transform(
            std::begin(m_lexer.tokens),
            std::end(m_lexer.tokens),
            std::back_inserter(actual),
            [](const auto &token) {
                return std_view(token.text);
            });

        EXPECT_EQ(expected, actual);
    }

    void expect_pos(PosVec &&expected) {
        PosVec actual;
        actual.reserve(m_lexer.tokens.size);
        std::transform(
            std::begin(m_lexer.tokens),
            std::end(m_lexer.tokens),
            std::back_inserter(actual),
            [](const auto &token) {
                return TokenPos{token.pos, token.lin, token.col};
            });

        EXPECT_EQ(expected, actual);
    }

private:
    void reportError() {
        StringBuilder sb{};
        sb << "Error message: " << m_lexer.err << "\n Tokens: { ";
        for (const auto &token : m_lexer.tokens) {
            sb << "'";
            string_escape(sb, token.text);
            sb << "', ";
        }
        sb << "}";
        LibcAllocator allocator;
        auto str = sb.moveStr(allocator);
        defer {
            allocator.free((void *)str.data);
        };
        LOG_ERR("%.*s", str.size, str.data);
        allocator.free((void *)m_lexer.err.data);
    }

    nkl::Lexer m_lexer{};
};

} // namespace

TEST_F(lexer, empty) {
    test_ok("", {}, {});
}

TEST_F(lexer, unknown) {
    test_err("`");
}

TEST_F(lexer, basic) {
    test_ok("2+2", {t_int_const, t_plus, t_int_const}, {"2", "+", "2"});
}

TEST_F(lexer, numeric) {
    test_ok("0", {t_int_const});
    test_err("0x");
    test_ok("0xFF", {t_int_const});
    test_ok("0x0123456789ABCDEF", {t_int_const});
    test_ok("0xdeadbeef", {t_int_const});
    test_err("0xG");
    test_ok("123", {t_int_const});
    test_err("123a");
    test_ok("123.", {t_float_const});
    test_ok(".123", {t_period, t_int_const}, {".", "123"});
    test_ok("0.0", {t_float_const});
    test_err("123e");
    test_ok("1e4", {t_float_const});
    test_ok("1e+4", {t_float_const});
    test_ok("1e-4", {t_float_const});
    test_ok("1.e-4", {t_float_const});
    test_ok(".1e+4", {t_period, t_float_const}, {".", "1e+4"});
    test_ok("1.0e+4", {t_float_const});
    test_err("1.0e+4a");
    test_err("1e+");
}

TEST_F(lexer, string) {
    test_ok(R"("")", {t_str_const}, {""});
    test_ok(R"("hello")", {t_str_const}, {"hello"});
    expect_pos({{0, 1, 1}, {7, 1, 8}});
    test_err(R"(")");
    test_err(R"("\)");
    test_err(R"("\")");
    test_err(R"("\z")");
    test_err(R"("
")");
    test_ok(
        R"("\n\t\0\\\"\
")",
        {t_escaped_str_const},
        {R"(\n\t\0\\\"\
)"});
    test_ok(R"("\"Hello, World!\n\"")", {t_escaped_str_const}, {R"(\"Hello, World!\n\")"});
    expect_pos({{0, 1, 1}, {21, 1, 22}});
}

TEST_F(lexer, positioning) {
    test_ok("2+2");
    expect_pos({{0, 1, 1}, {1, 1, 2}, {2, 1, 3}, {3, 1, 4}});
    test_ok("2 + 2");
    expect_pos({{0, 1, 1}, {2, 1, 3}, {4, 1, 5}, {5, 1, 6}});
    test_ok("2\n+\n2");
    expect_pos({{0, 1, 1}, {2, 2, 1}, {4, 3, 1}, {5, 3, 2}});
}

TEST_F(lexer, extra) {
    test_ok("x, y: f64 = 1e-6;");
    expect_id({t_id, t_comma, t_id, t_colon, t_f64, t_eq, t_float_const, t_semi});
    expect_text({"x", ",", "y", ":", "f64", "=", "1e-6", ";"});

    test_ok("func(arg1, arg2);");
    expect_id({t_id, t_par_l, t_id, t_comma, t_id, t_par_r, t_semi});
    expect_text({"func", "(", "arg1", ",", "arg2", ")", ";"});

    test_ok("struct {x: 0.0, y: 0.0}");
    // clang-format off
    expect_id({t_struct, t_brace_l, t_id, t_colon, t_float_const, t_comma, t_id, t_colon, t_float_const, t_brace_r});
    // clang-format on
    expect_text({"struct", "{", "x", ":", "0.0", ",", "y", ":", "0.0", "}"});

    test_ok("if a == b continue;");
    expect_id({t_if, t_id, t_eq_2x, t_id, t_continue, t_semi});
    expect_text({"if", "a", "==", "b", "continue", ";"});

    test_ok("ar[i] = !ar[i]");
    // clang-format off
    expect_id({t_id, t_bracket_l, t_id, t_bracket_r, t_eq, t_exclam, t_id, t_bracket_l, t_id, t_bracket_r});
    // clang-format on
    expect_text({"ar", "[", "i", "]", "=", "!", "ar", "[", "i", "]"});

    test_ok("mask |= flag | 0xFF & 1");
    expect_id({t_id, t_bar_eq, t_id, t_bar, t_int_const, t_amper, t_int_const});
    expect_text({"mask", "|=", "flag", "|", "0xFF", "&", "1"});

    test_ok("cast(f64) \"123.456\"");
    expect_id({t_cast, t_par_l, t_f64, t_par_r, t_str_const});
    expect_text({"cast", "(", "f64", ")", "123.456"});

    test_ok("cond ? a : b");
    expect_id({t_id, t_question, t_id, t_colon, t_id});
    expect_text({"cond", "?", "a", ":", "b"});

    test_ok("num <<= 1;");
    expect_id({t_id, t_less_2x_eq, t_int_const, t_semi});
    expect_text({"num", "<<=", "1", ";"});

    test_ok("fn (arg: ptr_t{void}) -> type {}");
    // clang-format off
    expect_id({t_fn, t_par_l, t_id, t_colon, t_ptr_t, t_brace_l, t_void, t_brace_r, t_par_r, t_minus_greater, t_id, t_brace_l, t_brace_r});
    // clang-format on
    expect_text({"fn", "(", "arg", ":", "ptr_t", "{", "void", "}", ")", "->", "type", "{", "}"});

    test_ok("ar := u8[__sizeof(var)];");
    // clang-format off
    expect_id({t_id, t_colon_eq, t_u8, t_bracket_l, t_id, t_par_l, t_id, t_par_r, t_bracket_r, t_semi});
    // clang-format on
    expect_text({"ar", ":=", "u8", "[", "__sizeof", "(", "var", ")", "]", ";"});
}
