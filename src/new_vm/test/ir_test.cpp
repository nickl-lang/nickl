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

TEST_F(ir, add) {
    auto p = nkir_createProgram();
    defer {
        nkir_deinitProgram(p);
    };

    auto i32_t = nkt_get_numeric(m_arena, Int32);

    nktype_t add_args[] = {i32_t, i32_t};
    auto args_t = nkt_get_tuple(m_arena, add_args, AR_SIZE(add_args), 1);

    auto add = nkir_makeFunct(p);
    nkir_startFunct(
        p, add, cs2s("add"), nkt_get_fn(m_arena, i32_t, args_t, nullptr, NkCallConv_Nk, false));
    nkir_startBlock(p, nkir_makeBlock(p), cs2s("start"));

    nkir_gen(p, nkir_make_add(nkir_makeRetRef(p), nkir_makeArgRef(p, 0), nkir_makeArgRef(p, 1)));
    nkir_gen(p, nkir_make_ret());

    {
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };

        nkir_inspect(p, sb);
        auto str = nksb_concat(sb);

        NK_LOG_INF("\n%.*s", str.size, str.data);
    }

    int32_t args[] = {4, 5};
    int32_t res = 0;

    nkir_invoke(p, add, nkval_t{&res, i32_t}, nkval_t{args, args_t});

    EXPECT_EQ(res, 9);
}
