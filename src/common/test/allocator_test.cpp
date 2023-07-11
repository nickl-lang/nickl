#include "nk/common/allocator.h"

#include <gtest/gtest.h>

class allocator : public testing::Test {
    void SetUp() override {
    }

    void TearDown() override {
    }

protected:
};

TEST_F(allocator, basic) {
    auto ptr = (int *)nk_alloc(nk_default_allocator, sizeof(int));
    *ptr = 42;
    nk_free(nk_default_allocator, ptr);
}
