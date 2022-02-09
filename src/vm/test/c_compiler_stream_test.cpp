#include "c_compiler_stream.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"
#include "nk/vm/vm.hpp"
#include "pipe_stream.hpp"

using namespace nk::vm;

namespace {

class c_compiler_stream : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        vm_init(VmConfig{});

        m_conf = {
            .compiler_binary = cstr_to_str(TEST_C_COMPILER),
            .additional_flags = cstr_to_str(TEST_C_COMPILER_FLAGS),
            .output_filename = string_format(
                *_mctx.tmp_allocator,
                TEST_FILES_DIR "%s_test.out",
                testing::UnitTest::GetInstance()->current_test_info()->name()),
            .quiet = TEST_QUIET,
        };
    }

    void TearDown() override {
        vm_deinit();
    }

protected:
    CCompilerConfig m_conf;
};

} // namespace

TEST_F(c_compiler_stream, empty) {
    auto src = ccompiler_streamOpen(m_conf);
    EXPECT_FALSE(ccompiler_streamClose(src));
}

TEST_F(c_compiler_stream, basic) {
    auto src = ccompiler_streamOpen(m_conf);

    src << "int main() {}";

    EXPECT_TRUE(ccompiler_streamClose(src));
}

TEST_F(c_compiler_stream, hello_world) {
    auto src = ccompiler_streamOpen(m_conf);

    src << "#include <stdio.h>\n"
           R"(int main() { printf("Hello, World!\n"); })";

    EXPECT_TRUE(ccompiler_streamClose(src));

    auto in = pipe_streamRead(m_conf.output_filename);
    DEFER({ pipe_streamClose(in); })

    std::string out_str{std::istreambuf_iterator<char>{in}, {}};
    EXPECT_EQ(out_str, "Hello, World!\n");
}

TEST_F(c_compiler_stream, undefined_var) {
    auto src = ccompiler_streamOpen(m_conf);

    src << "#include <stdio.h>\n"
           R"(int main() { printf("%i", var); })";

    EXPECT_FALSE(ccompiler_streamClose(src));
}
