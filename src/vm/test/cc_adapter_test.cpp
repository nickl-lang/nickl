#include "cc_adapter.hpp"

#include <filesystem>
#include <iterator>

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/vm/ir.h"
#include "pipe_stream.hpp"

namespace {

NK_LOG_USE_SCOPE(test);

class cc_adapter : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({});

        nksb_printf(
            &m_output_filename_sb,
            "%s%s_test" TEST_EXECUTABLE_EXT,
            std::filesystem::exists(TEST_FILES_DIR) ? TEST_FILES_DIR : "",
            testing::UnitTest::GetInstance()->current_test_info()->name());

        m_conf = {
            .compiler_binary = nk_mkstr(TEST_CC),
            .additional_flags = nk_mkstr(TEST_CC_FLAGS),
            .output_filename{m_output_filename_sb.data, m_output_filename_sb.size},
            .echo_src = false,
            .quiet = TEST_QUIET,
        };
    }

    void TearDown() override {
        nksb_free(&m_output_filename_sb);
    }

protected:
    std::string _runGetStdout() {
        auto in = nk_pipe_streamRead(m_conf.output_filename);
        defer {
            nk_pipe_streamClose(in);
        };

        std::string out_str{std::istreambuf_iterator<char>{in}, {}};
        NK_LOG_DBG("out_str=%s", out_str.c_str());
        return out_str;
    }

protected:
    NkStringBuilder_T m_output_filename_sb{};
    NkIrCompilerConfig m_conf;
};

} // namespace

TEST_F(cc_adapter, empty) {
    auto src = nkcc_streamOpen(m_conf);
    EXPECT_FALSE(nkcc_streamClose(src));
}

TEST_F(cc_adapter, empty_main) {
    auto src = nkcc_streamOpen(m_conf);

    src << "int main() {}\n";

    EXPECT_TRUE(nkcc_streamClose(src));

    EXPECT_EQ(_runGetStdout(), "");
}

TEST_F(cc_adapter, hello_world) {
    auto src = nkcc_streamOpen(m_conf);

    src << R"(
#include <stdio.h>
int main() {
    printf("Hello, World!");
}
)";

    EXPECT_TRUE(nkcc_streamClose(src));

    EXPECT_EQ(_runGetStdout(), "Hello, World!");
}

TEST_F(cc_adapter, undefined_var) {
    auto src = nkcc_streamOpen(m_conf);

    src << R"(
#include <stdio.h>"
int main() {
    printf("%i", var);
}
)";

    EXPECT_FALSE(nkcc_streamClose(src));
}
