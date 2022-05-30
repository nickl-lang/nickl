#include "nk/common/mem.hpp"

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
    LibcAllocator allocator;

    static constexpr size_t c_bytes_to_alloc = 100;

    uint8_t *mem = (uint8_t *)allocator.alloc(c_bytes_to_alloc);
    defer {
        allocator.free(mem);
    };

    for (size_t i = 0; i < c_bytes_to_alloc; i++) {
        mem[i] = i;
    }
}

TEST_F(mem, aligned) {
    LibcAllocator allocator;

    struct A {
        uint64_t u64;
        std::max_align_t _pad;
    };

    A *a = allocator.alloc<A>();
    defer {
        allocator.free_aligned(a);
    };

    a->u64 = 42;

    EXPECT_EQ((size_t)a % alignof(std::max_align_t), 0);
}
