#include "cc_adapter.hpp"

#include <filesystem>
#include <iterator>

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/pipe_stream.h"
#include "nk/common/stream.h"
#include "nk/common/string.hpp"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/vm/ir.h"

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
            .output_filename{nkav_init(m_output_filename_sb)},
            .quiet = TEST_QUIET,
        };
    }

    void TearDown() override {
        nksb_free(&m_output_filename_sb);
    }

protected:
    std::string runGetStdout() {
        auto in = nk_pipe_streamRead(m_conf.output_filename, false);
        defer {
            nk_pipe_streamClose(in);
        };

        NkStringBuilder sb{};
        defer {
            nksb_free(&sb);
        };
        nksb_readFromStream(&sb, in);
        NK_LOG_DBG("out_str=\"" nkstr_Fmt "\"", nkstr_Arg(sb));
        return std_str({nkav_init(sb)});
    }

protected:
    NkStringBuilder m_output_filename_sb{};
    NkIrCompilerConfig m_conf;
};

} // namespace

TEST_F(cc_adapter, empty) {
    auto src = nkcc_streamOpen(m_conf);
    EXPECT_TRUE(nkcc_streamClose(src));
}

TEST_F(cc_adapter, empty_main) {
    auto src = nkcc_streamOpen(m_conf);

    nk_stream_printf(src, "%s", "int main() {}\n");

    EXPECT_FALSE(nkcc_streamClose(src));

    EXPECT_EQ(runGetStdout(), "");
}

TEST_F(cc_adapter, hello_world) {
    auto src = nkcc_streamOpen(m_conf);

    nk_stream_printf(src, "%s", R"(
#include <stdio.h>
int main() {
    printf("Hello, World!");
}
)");

    EXPECT_FALSE(nkcc_streamClose(src));

    EXPECT_EQ(runGetStdout(), "Hello, World!");
}

TEST_F(cc_adapter, undefined_var) {
    auto src = nkcc_streamOpen(m_conf);

    nk_stream_printf(src, "%s", R"(
#include <stdio.h>
int main() {
    printf("%i", var);
}
)");

    EXPECT_TRUE(nkcc_streamClose(src));
}
