#include "cc_adapter.h"

#include <filesystem>

#include <gtest/gtest.h>

#include "ntk/arena.h"
#include "ntk/log.h"
#include "ntk/pipe_stream.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(test);

class vm_cc_adapter : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});

        nksb_printf(
            &m_output_filename_sb,
            "%s%s_test" TEST_EXECUTABLE_EXT,
            std::filesystem::exists(TEST_FILES_DIR) ? TEST_FILES_DIR : "./",
            testing::UnitTest::GetInstance()->current_test_info()->name());

        m_conf = {
            .compiler_binary = nk_cs2s(TEST_CC),
            .additional_flags = nk_cs2s(TEST_CC_FLAGS),
            .output_filename{NKS_INIT(m_output_filename_sb)},
            .quiet = TEST_QUIET,
        };
    }

    void TearDown() override {
        nk_arena_free(&m_scratch);
        nksb_free(&m_output_filename_sb);
    }

protected:
    std::string runGetStdout() {
        NkStream in;
        NkPipeStream ps;
        auto res = nk_pipe_streamOpenRead(
            {
                .ps = &ps,
                .scratch = &m_scratch,
                .cmd = m_conf.output_filename,
                .opt_buf = {},
                .quiet = false,
            },
            &in);
        defer {
            nk_pipe_streamClose(&ps);
        };

        EXPECT_TRUE(res);

        if (res) {
            NkStringBuilder sb{};
            defer {
                nksb_free(&sb);
            };
            EXPECT_TRUE(nksb_readFromStream(&sb, in));
            NK_LOG_DBG("out_str=\"" NKS_FMT "\"", NKS_ARG(sb));
            return nk_s2stdStr({NKS_INIT(sb)});
        } else {
            return "";
        }
    }

protected:
    NkArena m_scratch{};
    NkStringBuilder m_output_filename_sb{};
    NkIrCompilerConfig m_conf;
};

} // namespace

TEST_F(vm_cc_adapter, empty) {
    NkStream src;
    NkPipeStream ps;
    nkcc_streamOpen(&m_scratch, &ps, {}, m_conf, &src);
    int ret = nkcc_streamClose(&ps);
    EXPECT_TRUE(ret);
}

TEST_F(vm_cc_adapter, empty_main) {
    NkStream src;
    NkPipeStream ps;
    nkcc_streamOpen(&m_scratch, &ps, {}, m_conf, &src);

    nk_printf(src, "%s", "int main() {}\n");

    EXPECT_FALSE(nkcc_streamClose(&ps));

    EXPECT_EQ(runGetStdout(), "");
}

TEST_F(vm_cc_adapter, hello_world) {
    NkStream src;
    NkPipeStream ps;
    nkcc_streamOpen(&m_scratch, &ps, {}, m_conf, &src);

    nk_printf(src, "%s", R"(
#include <stdio.h>
int main() {
    printf("Hello, World!");
}
)");

    EXPECT_FALSE(nkcc_streamClose(&ps));

    EXPECT_EQ(runGetStdout(), "Hello, World!");
}

TEST_F(vm_cc_adapter, undefined_var) {
    NkStream src;
    NkPipeStream ps;
    nkcc_streamOpen(&m_scratch, &ps, {}, m_conf, &src);

    nk_printf(src, "%s", R"(
#include <stdio.h>
int main() {
    printf("%i", var);
}
)");

    EXPECT_TRUE(nkcc_streamClose(&ps));
}
