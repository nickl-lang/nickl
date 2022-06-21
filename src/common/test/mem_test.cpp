#include "nk/common/mem.h"

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/utils.hpp"

class mem : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

TEST_F(mem, basic) {
    static constexpr size_t c_bytes_to_alloc = 100;

    uint8_t *mem = (uint8_t *)platform_alloc(c_bytes_to_alloc);
    defer {
        platform_free(mem);
    };

    for (size_t i = 0; i < c_bytes_to_alloc; i++) {
        mem[i] = i;
    }
}

TEST_F(mem, aligned) {
    struct A {
        uint64_t u64;
        std::max_align_t _pad;
    };

    A *a = (A *)platform_alloc_aligned(sizeof(A), alignof(A));
    defer {
        platform_free_aligned(a);
    };

    a->u64 = 42;

    EXPECT_EQ((size_t)a % alignof(std::max_align_t), 0);
}
