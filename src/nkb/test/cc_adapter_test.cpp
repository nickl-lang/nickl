#include "cc_adapter.h"

#include <filesystem>
#include <iterator>

#include <gtest/gtest.h>

#include "nkb/ir.h"
#include "ntk/logger.h"
#include "ntk/pipe_stream.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

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

        static nks const additional_flags = nk_cs2s(TEST_CC_FLAGS);
        m_conf = NkIrCompilerConfig{
            .compiler_binary = nk_cs2s(TEST_CC),
            .additional_flags{&additional_flags, 1},
            .output_filename{nkav_init(m_output_filename_sb)},
            .output_kind = NkbOutput_Executable,
            .quiet = TEST_QUIET,
        };
    }

    void TearDown() override {
        nksb_free(&m_output_filename_sb);
    }

protected:
    std::string runGetStdout() {
        nk_stream in;
        auto res = nk_pipe_streamRead(&in, m_conf.output_filename, false);
        defer {
            nk_pipe_streamClose(in);
        };

        EXPECT_TRUE(res);

        if (res) {
            NkStringBuilder sb{};
            defer {
                nksb_free(&sb);
            };
            nksb_readFromStream(&sb, in);
            NK_LOG_DBG("out_str=\"" nks_Fmt "\"", nks_Arg(sb));
            return std_str({nkav_init(sb)});
        } else {
            return "";
        }
    }

protected:
    NkStringBuilder m_output_filename_sb{};
    NkIrCompilerConfig m_conf;
};

} // namespace

TEST_F(cc_adapter, empty) {
    nk_stream src;
    auto res = nkcc_streamOpen(&src, m_conf);
    EXPECT_TRUE(res);

    EXPECT_TRUE(nkcc_streamClose(src));
}

TEST_F(cc_adapter, empty_main) {
    nk_stream src;
    auto res = nkcc_streamOpen(&src, m_conf);
    EXPECT_TRUE(res);

    if (res) {
        nk_printf(src, "%s", "int main() {}\n");
    }

    EXPECT_FALSE(nkcc_streamClose(src));

    EXPECT_EQ(runGetStdout(), "");
}

TEST_F(cc_adapter, hello_world) {
    nk_stream src;
    auto res = nkcc_streamOpen(&src, m_conf);
    EXPECT_TRUE(res);

    if (res) {
        nk_printf(src, "%s", R"(
#include <stdio.h>
int main() {
    printf("Hello, World!");
}
)");
    }

    EXPECT_FALSE(nkcc_streamClose(src));

    EXPECT_EQ(runGetStdout(), "Hello, World!");
}

TEST_F(cc_adapter, undefined_var) {
    nk_stream src;
    auto res = nkcc_streamOpen(&src, m_conf);
    EXPECT_TRUE(res);

    if (res) {
        nk_printf(src, "%s", R"(
#include <stdio.h>
int main() {
    printf("%i", var);
}
)");
    }

    EXPECT_TRUE(nkcc_streamClose(src));
}
