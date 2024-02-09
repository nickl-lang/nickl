#include "ntk/log_array.hpp"

#include <cstring>

#include <gtest/gtest.h>

#include "ntk/logger.h"
#include "ntk/utils.h"

class LogArray : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({});
    }

    void TearDown() override {
    }
};

TEST_F(LogArray, basic) {
    using ValueType = u64;

    NkLogArray<ValueType> ar{};
    defer {
        ar.deinit();
    };

    ar.reserve(1);

    EXPECT_EQ(ar.size(), 0);

    *ar.push() = 4;
    *ar.push() = 5;
    *ar.push() = 42;

    EXPECT_EQ(ar.size(), 3);

    EXPECT_EQ(ar.at(0), 4);
    EXPECT_EQ(ar.at(1), 5);
    EXPECT_EQ(ar.at(2), 42);
}

TEST_F(LogArray, many_small_pushes) {
    static constexpr usize c_ic = 64;
    static constexpr usize c_bytes_to_push = 1024;

    NkLogArray<u8> ar{};
    defer {
        ar.deinit();
    };

    ar.reserve(c_ic);

    for (usize i = 0; i < c_bytes_to_push; i++) {
        ar.push();
    }
    EXPECT_EQ(ar.size(), c_bytes_to_push);
}

TEST_F(LogArray, one_big_push) {
    static constexpr usize c_ic = 64;
    static constexpr usize c_bytes_to_push = 1024;

    NkLogArray<u8> ar{};
    defer {
        ar.deinit();
    };

    ar.reserve(c_ic);

    ar.push(c_bytes_to_push);
    EXPECT_EQ(ar.size(), 2 * c_bytes_to_push - ceilToPowerOf2(c_ic));
}

TEST_F(LogArray, pop) {
    static constexpr usize c_bytes_to_push = 64;

    {
        NkLogArray<u8> ar{};
        defer {
            ar.deinit();
        };

        ar.reserve(1);

        ar.push(c_bytes_to_push);
        usize size1 = ar.size();
        EXPECT_EQ(size1, c_bytes_to_push * 2 - 1);

        ar.pop(ar.size());

        ar.push(c_bytes_to_push);
        usize size2 = ar.size();
        EXPECT_EQ(size2, c_bytes_to_push * 2 - 1);
    }

    {
        NkLogArray<u8> ar{};
        defer {
            ar.deinit();
        };

        ar.reserve(1);

        for (usize i = 0; i < c_bytes_to_push; i++) {
            ar.push();
        }
        usize size1 = ar.size();
        EXPECT_EQ(size1, c_bytes_to_push);

        ar.pop(ar.size());

        for (usize i = 0; i < c_bytes_to_push; i++) {
            ar.push();
        }
        usize size2 = ar.size();
        EXPECT_EQ(size2, c_bytes_to_push);
    }

    {
        NkLogArray<u8> ar{};
        defer {
            ar.deinit();
        };

        ar.reserve(32);

        ar.push(32);
        EXPECT_EQ(ar.size(), 32);
        ar.push(64);
        EXPECT_EQ(ar.size(), 96);
        ar.pop(64);
        EXPECT_EQ(ar.size(), 32);
        ar.push(64);
        EXPECT_EQ(ar.size(), 96);
    }

    {
        NkLogArray<u8> ar{};
        defer {
            ar.deinit();
        };

        ar.reserve(32);

        ar.push(32);
        EXPECT_EQ(ar.size(), 32);
        ar.pop(32);
        EXPECT_EQ(ar.size(), 0);
        ar.push(32);
        EXPECT_EQ(ar.size(), 32);
        ar.pop(32);
        EXPECT_EQ(ar.size(), 0);
    }
}

TEST_F(LogArray, zero_init) {
    NkLogArray<u8> ar{};
    defer {
        ar.deinit();
    };

    *ar.push() = 42;
    EXPECT_EQ(ar.size(), 1);
    EXPECT_EQ(ar[0], 42);
}

// TODO Rewrite LogArray to support split push

// TEST_F(LogArray, multiple_reserves) {
//     NkLogArray<int> ar{};
//     defer {
//         ar.deinit();
//     };

//     usize sz = 1;

//     ar.reserve(sz *= 10);
//     ar.reserve(sz *= 10);
//     ar.reserve(sz *= 10);

//     auto data = ar.push(sz);
//     std::memset(data.data(), 0, sz * sizeof(decltype(ar)::value_type));

//     EXPECT_EQ(ar.size(), sz);
// }
