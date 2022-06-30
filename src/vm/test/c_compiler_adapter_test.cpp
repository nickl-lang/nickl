#include "c_compiler_adapter.hpp"

#include <iterator>

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/static_string_builder.hpp"
#include "nk/common/utils.hpp"
#include "pipe_stream.hpp"

using namespace nk::vm;
using namespace nk;

namespace {

LOG_USE_SCOPE(test::c_compiler_adapter);

class c_compiler_adapter : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        static char buf[1024];
        Slice<char> output_filename{buf, sizeof(buf) / sizeof(buf[0])};
        StaticStringBuilder sb{output_filename};
        sb.printf(
            TEST_FILES_DIR "%s_test.out",
            testing::UnitTest::GetInstance()->current_test_info()->name());

        m_conf = {
            .compiler_binary = cs2s(TEST_C_COMPILER),
            .additional_flags = cs2s(TEST_C_COMPILER_FLAGS),
            .output_filename = output_filename,
            .quiet = TEST_QUIET,
        };
    }

    void TearDown() override {
    }

protected:
    std::string _runGetStdout() {
        auto in = pipe_streamRead(m_conf.output_filename);
        defer {
            pipe_streamClose(in);
        };

        std::string out_str{std::istreambuf_iterator<char>{in}, {}};
        LOG_DBG("out_str=%s", out_str.c_str());
        return out_str;
    }

protected:
    CCompilerConfig m_conf;
};

} // namespace

TEST_F(c_compiler_adapter, empty) {
    auto src = c_compiler_streamOpen(m_conf);
    EXPECT_FALSE(c_compiler_streamClose(src));
}

TEST_F(c_compiler_adapter, empty_main) {
    auto src = c_compiler_streamOpen(m_conf);

    src << "int main() {}\n";

    EXPECT_TRUE(c_compiler_streamClose(src));
}

TEST_F(c_compiler_adapter, hello_world) {
    auto src = c_compiler_streamOpen(m_conf);

    src << R"(
#include <stdio.h>
int main() {
    printf("Hello, World!");
}
)";

    EXPECT_TRUE(c_compiler_streamClose(src));

    EXPECT_EQ(_runGetStdout(), "Hello, World!");
}

TEST_F(c_compiler_adapter, undefined_var) {
    auto src = c_compiler_streamOpen(m_conf);

    src << R"(
#include <stdio.h>"
int main() {
    printf("%i", var);
}
)";

    EXPECT_FALSE(c_compiler_streamClose(src));
}