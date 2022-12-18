#include "nk/vm/ir.h"

#include <iostream>

#include <gtest/gtest.h>

#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/string.hpp"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"
#include "nk/vm/value.h"

namespace {

NK_LOG_USE_SCOPE(test);

class ir : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({});

        m_arena = nk_create_arena();
    }

    void TearDown() override {
        nk_free_arena(m_arena);
    }

protected:
    NkAllocator *m_arena;
};

} // namespace

TEST_F(ir, basic) {
    auto ir = nkir_createProgram();
    defer {
        nkir_deinitProgram(ir);
    };

    nkir_startFunct(
        ir,
        nkir_makeFunct(ir),
        cs2s("test"),
        nkt_get_fn(
            m_arena,
            nkt_get_void(m_arena),
            nkt_get_tuple(m_arena, nullptr, 0, 1),
            nullptr,
            NkCallConv_Nk,
            false));
    nkir_startBlock(ir, nkir_makeBlock(ir), cs2s("start"));

    nkir_gen(ir, nkir_make_ret());

    auto sb = nksb_create();
    defer {
        nksb_free(sb);
    };

    nkir_inspect(ir, sb);
    auto str = nksb_concat(sb);

    NK_LOG_INF("\n%.*s", str.size, str.data);
}
