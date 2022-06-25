#include "nk/common/file_utils.hpp"

#include <cstring>
#include <filesystem>

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/stack_allocator.hpp"
#include "nk/common/utils.hpp"

class file_utils : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
        m_arena.deinit();
    }

protected:
    StackAllocator m_arena{};
};

TEST_F(file_utils, read_file) {
    auto ar = file_read(cs2s(TEST_FILE_NAME), m_arena);

    static constexpr const char *c_test_str = "Hello, World!\n";

    ASSERT_TRUE(ar.data);
    ASSERT_EQ(std::strlen(c_test_str), ar.size);
    EXPECT_EQ(std::strncmp((char *)ar.data, c_test_str, ar.size), 0);
}

TEST_F(file_utils, read_file_nonexistent) {
    auto ar = file_read(cs2s("nonexistent_file.txt"), m_arena);

    ASSERT_FALSE(ar.data);
    EXPECT_EQ(0, ar.size);
}

TEST_F(file_utils, big_file) {
    auto ar = file_read(cs2s(BIG_FILE_NAME), m_arena);

    ASSERT_TRUE(ar.data);
    EXPECT_NE(0, ar.size);
    ASSERT_EQ(ar.size, std::filesystem::file_size(BIG_FILE_NAME));
    EXPECT_EQ(ar.size, std::count(ar.data, ar.data + ar.size, 1));
}
