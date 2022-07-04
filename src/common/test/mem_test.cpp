#include "nk/mem/mem.h"

#include <gtest/gtest.h>

#include "nk/utils/logger.h"
#include "nk/utils/utils.hpp"

using namespace nk;

class mem : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

TEST_F(mem, basic) {
    static constexpr size_t c_bytes_to_alloc = 100;

    uint8_t *mem = (uint8_t *)nk_platform_alloc(c_bytes_to_alloc);
    defer {
        nk_platform_free(mem);
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

    A *a = (A *)nk_platform_alloc_aligned(sizeof(A), alignof(A));
    defer {
        nk_platform_free_aligned(a);
    };

    a->u64 = 42;

    EXPECT_EQ((size_t)a % alignof(std::max_align_t), 0);
}
