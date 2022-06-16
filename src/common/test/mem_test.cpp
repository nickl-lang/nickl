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
    static constexpr size_t c_bytes_to_alloc = 100;

    uint8_t *mem = (uint8_t *)platform_alloc(c_bytes_to_alloc);
    defer {
        platform_free(mem);
    };

    for (size_t i = 0; i < c_bytes_to_alloc; i++) {
        mem[i] = i;
    }
}
