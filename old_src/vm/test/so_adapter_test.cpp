#include "so_adapter.hpp"

#include <iterator>

#include <gtest/gtest.h>

#include "nk/utils/logger.h"
#include "nk/utils/utils.hpp"

namespace {

using namespace nk::vm;
using namespace nk;

class so_adapter : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }

protected:
};

} // namespace

TEST_F(so_adapter, basic) {
    void *so = openSharedObject(cs2s(TESTLIB));
    ASSERT_NE(so, nullptr);
    defer {
        closeSharedObject(so);
    };

    auto s_test_printf_called = (bool *)resolveSym(so, cs2s("s_test_printf_called"));
    ASSERT_NE(s_test_printf_called, nullptr);

    EXPECT_FALSE(*s_test_printf_called);

    auto test_printf = (int (*)(char const *, ...))resolveSym(so, cs2s("test_printf"));
    ASSERT_NE(test_printf, nullptr);

    test_printf("Hello, %s!\n", "World");

    EXPECT_TRUE(*s_test_printf_called);
}

TEST_F(so_adapter, nonexistent) {
    void *so = openSharedObject(cs2s("test_nonexistent_library_path"));
    EXPECT_EQ(so, nullptr);
}
