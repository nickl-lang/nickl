#include "nk/common/sequence.hpp"

#include <gtest/gtest.h>

#include "nk/common/utils.hpp"

class sequence : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

TEST_F(sequence, basic) {
    using ValueType = uint64_t;

    auto sq = Sequence<ValueType>::create(1);
    DEFER({ sq.deinit(); });

    EXPECT_EQ(sq.size, 0);

    sq.push() = 4;
    sq.push() = 5;
    sq.push() = 42;

    EXPECT_EQ(sq.size, 3);

    EXPECT_EQ(sq.at(0), 4);
    EXPECT_EQ(sq.at(1), 5);
    EXPECT_EQ(sq.at(2), 42);
}

TEST_F(sequence, many_small_pushes) {
    static constexpr size_t c_ic = 64;
    static constexpr size_t c_bytes_to_push = 1024;

    auto sq = Sequence<uint8_t>::create(c_ic);
    DEFER({ sq.deinit(); });

    for (size_t i = 0; i < c_bytes_to_push; i++) {
        sq.push();
    }
    EXPECT_EQ(sq.size, c_bytes_to_push);
}

TEST_F(sequence, one_big_push) {
    static constexpr size_t c_ic = 64;
    static constexpr size_t c_bytes_to_push = 1024;

    auto sq = Sequence<uint8_t>::create(c_ic);
    DEFER({ sq.deinit(); });

    sq.push(c_bytes_to_push);
    EXPECT_EQ(sq.size, 2 * c_bytes_to_push - ceilToPowerOf2(c_ic));
}

TEST_F(sequence, pop) {
    static constexpr size_t c_bytes_to_push = 64;

    {
        auto sq = Sequence<uint8_t>::create(1);
        DEFER({ sq.deinit(); });

        sq.push(c_bytes_to_push);
        size_t size1 = sq.size;
        EXPECT_EQ(size1, c_bytes_to_push * 2 - 1);

        sq.pop(sq.size);

        sq.push(c_bytes_to_push);
        size_t size2 = sq.size;
        EXPECT_EQ(size2, c_bytes_to_push * 2 - 1);
    }

    {
        auto sq = Sequence<uint8_t>::create(1);
        DEFER({ sq.deinit(); });

        for (size_t i = 0; i < c_bytes_to_push; i++) {
            sq.push();
        }
        size_t size1 = sq.size;
        EXPECT_EQ(size1, c_bytes_to_push);

        sq.pop(sq.size);

        for (size_t i = 0; i < c_bytes_to_push; i++) {
            sq.push();
        }
        size_t size2 = sq.size;
        EXPECT_EQ(size2, c_bytes_to_push);
    }

    {
        auto sq = Sequence<uint8_t>::create(32);
        DEFER({ sq.deinit(); });

        sq.push(32);
        EXPECT_EQ(sq.size, 32);
        sq.push(64);
        EXPECT_EQ(sq.size, 96);
        sq.pop(64);
        EXPECT_EQ(sq.size, 32);
        sq.push(64);
        EXPECT_EQ(sq.size, 96);
    }

    {
        auto sq = Sequence<uint8_t>::create(32);
        DEFER({ sq.deinit(); });

        sq.push(32);
        EXPECT_EQ(sq.size, 32);
        sq.pop(32);
        EXPECT_EQ(sq.size, 0);
        sq.push(32);
        EXPECT_EQ(sq.size, 32);
        sq.pop(32);
        EXPECT_EQ(sq.size, 0);
    }
}
