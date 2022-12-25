#include "nk/ds/arena.hpp"

#include <cstring>

#include <gtest/gtest.h>

#include "nk/str/string.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/utils.hpp"

using namespace nk;

class arena : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
        m_arena.deinit();
    }

protected:
    Arena<uint8_t> m_arena{};
};

TEST_F(arena, push) {
    auto const c_test_str = cs2s("hello world");

    char *const mem = (char *)m_arena.push(c_test_str.size).begin();
    Slice<char> const str{mem, c_test_str.size};
    c_test_str.copy(str);

    EXPECT_EQ(m_arena.size, c_test_str.size);
    EXPECT_EQ(std_str(c_test_str), std_str(str));
}

TEST_F(arena, push_zero) {
    auto res = m_arena.push(0);

    EXPECT_EQ(m_arena.size, 0);

    EXPECT_EQ(res.size, 0);
    EXPECT_EQ(res.data, nullptr);
}

TEST_F(arena, pop) {
    m_arena.push(42);
    EXPECT_EQ(m_arena.size, 42);

    m_arena.pop(2);
    EXPECT_EQ(m_arena.size, 40);

    m_arena.pop(21);
    EXPECT_EQ(m_arena.size, 19);

    m_arena.pop(19);
    EXPECT_EQ(m_arena.size, 0);
}

TEST_F(arena, clear) {
    m_arena.push(42);
    EXPECT_EQ(m_arena.size, 42);

    m_arena.clear();
    EXPECT_EQ(m_arena.size, 0);
}

TEST_F(arena, copy_empty) {
    m_arena.copy(nullptr);
}

TEST_F(arena, copy_empty_after_clear) {
    m_arena.push();
    m_arena.clear();

    m_arena.copy(nullptr);
}

TEST_F(arena, clear_multiple_blocks) {
    static constexpr size_t c_test_size = 1024;

    for (size_t i = 0; i < c_test_size; i++) {
        *m_arena.push() = 0;
    }
    EXPECT_EQ(m_arena.size, c_test_size);

    m_arena.clear();
    EXPECT_EQ(m_arena.size, 0);

    auto mem = m_arena.push(c_test_size);
    EXPECT_EQ(m_arena.size, c_test_size);
    std::memset(mem.data, 0, mem.size);
}

TEST_F(arena, push_pop_push) {
    uint8_t *data = m_arena.push(10);
    EXPECT_EQ(m_arena.size, 10);
    std::memset(data, 'a', 10);

    data = m_arena.push(50);
    EXPECT_EQ(m_arena.size, 60);
    std::memset(data, 'b', 50);

    m_arena.pop(51);
    EXPECT_EQ(m_arena.size, 9);

    data = m_arena.push(10);
    EXPECT_EQ(m_arena.size, 19);
    std::memset(data, 'c', 10);

    char buf[4096] = {};
    Slice<char> str{buf, m_arena.size};
    m_arena.copy((uint8_t *)str.data);

    EXPECT_EQ(std_str(str), "aaaaaaaaacccccccccc");
}

TEST_F(arena, align) {
    size_t const c_align = 3;

    Arena<double> ar{};
    defer {
        ar.deinit();
    };

    auto ptr = ar.push_aligned(c_align);
    EXPECT_EQ((size_t)ptr.data % (c_align * alignof(decltype(ar)::value_type)), 0);
}

TEST_F(arena, multiple_reserves) {
    size_t sz = 1;

    m_arena.reserve(sz *= 10);
    m_arena.reserve(sz *= 10);
    m_arena.reserve(sz *= 10);

    auto data = m_arena.push(sz);
    std::memset(data.data, 0, sz);

    EXPECT_EQ(m_arena.size, sz);
}
