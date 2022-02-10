#include "nkl/core/lexer.hpp"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "nk/common/logger.hpp"
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
        ASSERT_FALSE(m_lexer.lex(cs2s(src)));
        reportError();
    }

    void expect_id(IDVec &&expected) {
        expected.emplace_back(Token_eof);

        IDVec actual;
        actual.reserve(m_lexer.tokens.size);
        std::transform(
            std::begin(m_lexer.tokens),
            std::end(m_lexer.tokens),
            std::back_inserter(actual),
            [](const auto &token) {
                return token.id;
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
        LOG_INF(
            "Error message: %.*s\n Tokens: { %s}",
            m_lexer.err.size,
            m_lexer.err.data,
            [&]() {
                std::ostringstream ss;
                for (const auto &token : m_lexer.tokens) {
                    ss << "'" << token.text << "', ";
                }
                return ss.str();
            }()
                .data());
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
    test_ok("2+2", {Token_int_const, Token_plus, Token_int_const}, {"2", "+", "2"});
}

TEST_F(lexer, numeric) {
    test_ok("0", {Token_int_const});
    test_err("0x");
    test_ok("0xFF", {Token_int_const});
    test_ok("0x0123456789ABCDEF", {Token_int_const});
    test_ok("0xdeadbeef", {Token_int_const});
    test_err("0xG");
    test_ok("123", {Token_int_const});
    test_err("123a");
    test_ok("123.", {Token_float_const});
    test_ok(".123", {Token_float_const});
    test_ok("0.0", {Token_float_const});
    test_err("123e");
    test_ok("1e4", {Token_float_const});
    test_ok("1e+4", {Token_float_const});
    test_ok("1e-4", {Token_float_const});
    test_ok("1.e-4", {Token_float_const});
    test_ok(".1e+4", {Token_float_const});
    test_ok("1.0e+4", {Token_float_const});
    test_err("1.0e+4a");
    test_err("1e+");
}

TEST_F(lexer, string) {
    test_ok("\"\"", {Token_str_const}, {""});
    test_ok("\"hello\"", {Token_str_const}, {"hello"});
    expect_pos({{0, 1, 1}, {7, 1, 8}});
    test_err("\"");
    test_err("\"\\");
    test_err("\"\\\"");
    test_err("\"\\z\"");
    test_err("\"\n\"");
    test_ok("\"\\n\\t\\0\\\\\\\"\\\n\"", {Token_str_const}, {"\\n\\t\\0\\\\\\\"\\\n"});
    test_ok("\"\\\"Hello, World!\\n\\\"\"", {Token_str_const}, {"\\\"Hello, World!\\n\\\""});
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
    expect_id(
        {Token_id,
         Token_comma,
         Token_id,
         Token_colon,
         Token_f64,
         Token_eq,
         Token_float_const,
         Token_semi});
    expect_text({"x", ",", "y", ":", "f64", "=", "1e-6", ";"});

    test_ok("func(arg1, arg2);");
    expect_id({Token_id, Token_par_l, Token_id, Token_comma, Token_id, Token_par_r, Token_semi});
    expect_text({"func", "(", "arg1", ",", "arg2", ")", ";"});

    test_ok("struct {x: 0.0, y: 0.0}");
    expect_id(
        {Token_struct,
         Token_brace_l,
         Token_id,
         Token_colon,
         Token_float_const,
         Token_comma,
         Token_id,
         Token_colon,
         Token_float_const,
         Token_brace_r});
    expect_text({"struct", "{", "x", ":", "0.0", ",", "y", ":", "0.0", "}"});

    test_ok("if a == b continue;");
    expect_id({Token_if, Token_id, Token_eq_dbl, Token_id, Token_continue, Token_semi});
    expect_text({"if", "a", "==", "b", "continue", ";"});

    test_ok("ar[i] = !ar[i]");
    expect_id(
        {Token_id,
         Token_bracket_l,
         Token_id,
         Token_bracket_r,
         Token_eq,
         Token_exclam,
         Token_id,
         Token_bracket_l,
         Token_id,
         Token_bracket_r});
    expect_text({"ar", "[", "i", "]", "=", "!", "ar", "[", "i", "]"});

    test_ok("mask |= flag | 0xFF & 1");
    expect_id(
        {Token_id,
         Token_bar_eq,
         Token_id,
         Token_bar,
         Token_int_const,
         Token_amper,
         Token_int_const});
    expect_text({"mask", "|=", "flag", "|", "0xFF", "&", "1"});

    test_ok("cast(f64) \"123.456\"");
    expect_id({Token_cast, Token_par_l, Token_f64, Token_par_r, Token_str_const});
    expect_text({"cast", "(", "f64", ")", "123.456"});

    test_ok("cond ? a : b");
    expect_id({Token_id, Token_question, Token_id, Token_colon, Token_id});
    expect_text({"cond", "?", "a", ":", "b"});

    test_ok("num <<= 1;");
    expect_id({Token_id, Token_less_dbl_eq, Token_int_const, Token_semi});
    expect_text({"num", "<<=", "1", ";"});

    test_ok("fn (arg: ptr_t{void}) -> type {}");
    expect_id(
        {Token_fn,
         Token_par_l,
         Token_id,
         Token_colon,
         Token_ptr_t,
         Token_brace_l,
         Token_void,
         Token_brace_r,
         Token_par_r,
         Token_minus_greater,
         Token_id,
         Token_brace_l,
         Token_brace_r});
    expect_text({"fn", "(", "arg", ":", "ptr_t", "{", "void", "}", ")", "->", "type", "{", "}"});

    test_ok("ar := u8[__sizeof(var)];");
    expect_id(
        {Token_id,
         Token_colon_eq,
         Token_u8,
         Token_bracket_l,
         Token_id,
         Token_par_l,
         Token_id,
         Token_par_r,
         Token_bracket_r,
         Token_semi});
    expect_text({"ar", ":=", "u8", "[", "__sizeof", "(", "var", ")", "]", ";"});
}
