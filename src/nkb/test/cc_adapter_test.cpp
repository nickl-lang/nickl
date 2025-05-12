#include "cc_adapter.h"

#include <filesystem>

#include <gtest/gtest.h>

#include "nkb/ir.h"
#include "ntk/log.h"
#include "ntk/pipe_stream.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(test);

class cc_adapter : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});

        nksb_printf(
            &m_output_filename_sb,
            "%s%s_test" TEST_EXECUTABLE_EXT,
            std::filesystem::exists(TEST_FILES_DIR) ? TEST_FILES_DIR : "./",
            testing::UnitTest::GetInstance()->current_test_info()->name());

        static NkString const additional_flags = nk_cs2s(TEST_CC_FLAGS);
        m_conf = NkIrCompilerConfig{
            .compiler_binary = nk_cs2s(TEST_CC),
            .additional_flags{&additional_flags, 1},
            .output_filename{NKS_INIT(m_output_filename_sb)},
            .output_kind = NkbOutput_Executable,
            .quiet = TEST_QUIET,
        };
    }

    void TearDown() override {
        nksb_free(&m_output_filename_sb);
    }

protected:
    std::string runGetStdout() {
        NkPipeStream in;
        auto res = nk_pipe_streamOpenRead(&in, m_conf.output_filename, false);
        defer {
            nk_pipe_streamClose(&in);
        };

        EXPECT_TRUE(res);

        if (res) {
            NkStringBuilder sb{};
            defer {
                nksb_free(&sb);
            };
            EXPECT_TRUE(nksb_readFromStream(&sb, in.stream));
            NK_LOG_DBG("out_str=\"" NKS_FMT "\"", NKS_ARG(sb));
            return nk_s2stdStr({NKS_INIT(sb)});
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
    NkPipeStream src;
    auto res = nkcc_streamOpen(&src, m_conf);
    EXPECT_TRUE(res);

    EXPECT_TRUE(nkcc_streamClose(&src));
}

TEST_F(cc_adapter, empty_main) {
    NkPipeStream src;
    auto res = nkcc_streamOpen(&src, m_conf);
    EXPECT_TRUE(res);

    if (res) {
        nk_printf(src.stream, "%s", "int main() {}\n");
    }

    EXPECT_FALSE(nkcc_streamClose(&src));

    EXPECT_EQ(runGetStdout(), "");
}

TEST_F(cc_adapter, hello_world) {
    NkPipeStream src;
    auto res = nkcc_streamOpen(&src, m_conf);
    EXPECT_TRUE(res);

    if (res) {
        nk_printf(src.stream, "%s", R"(
#include <stdio.h>
int main() {
    printf("Hello, World!");
}
)");
    }

    EXPECT_FALSE(nkcc_streamClose(&src));

    EXPECT_EQ(runGetStdout(), "Hello, World!");
}

TEST_F(cc_adapter, undefined_var) {
    NkPipeStream src;
    auto res = nkcc_streamOpen(&src, m_conf);
    EXPECT_TRUE(res);

    if (res) {
        nk_printf(src.stream, "%s", R"(
#include <stdio.h>
int main() {
    printf("%i", var);
}
)");
    }

    EXPECT_TRUE(nkcc_streamClose(&src));
}
