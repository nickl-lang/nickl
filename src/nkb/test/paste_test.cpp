#include <gtest/gtest.h>

#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/log.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(test);

class ir_paste : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});
    }

    void TearDown() override {
        nk_arena_free(&m_tmp_arena);
        nk_arena_free(&m_arena);
    }

protected:
    NkArena m_arena{};
    NkArena m_tmp_arena{};
};

} // namespace

TEST_F(ir_paste, defer_scenario) {
    u32 type_id = 0;

    NkIrType const i64_t{
        .as{.num{Int64}},
        .size = 8,
        .flags = 0,
        .align = 8,
        .kind = NkIrType_Numeric,
        .id = type_id++,
    };

    NkIrProcInfo const proc_info{
        .args_t{},
        .ret_t = &i64_t,
        .call_conv = NkCallConv_Nk,
        .flags = 0,
    };
    NkIrType const proc_t{
        .as{.proc{proc_info}},
        .size = 8,
        .flags = 0,
        .align = 8,
        .kind = NkIrType_Procedure,
        .id = type_id++,
    };

    // Build

    auto ir = nkir_createProgram(&m_arena);
    auto proc = nkir_createProc(ir);

    nkir_startProc(ir, proc, nk_cs2atom("foo"), &proc_t, {}, NK_ATOM_INVALID, 0, NkIrVisibility_Default);

    auto const start = nkir_make_label(nkir_createLabel(ir, nk_cs2atom("@start")));
    nkir_gen(ir, {&start, 1});

    auto const cnst = nkir_makeRodata(ir, NK_ATOM_INVALID, &i64_t, NkIrVisibility_Local);
    *(int64_t *)nkir_getDataPtr(ir, cnst) = 42;
    auto const mov = nkir_make_mov(ir, nkir_makeRetRef(ir), nkir_makeDataRef(ir, cnst));
    nkir_gen(ir, {&mov, 1});

    auto const ret = nkir_make_ret(ir);
    nkir_gen(ir, {&ret, 1});

    nkir_finishProc(ir, proc, 0);

    // Inspect

#ifdef ENABLE_LOGGING
    NkStringBuilder sb{};
    sb.alloc = nk_arena_getAllocator(&m_tmp_arena);
    nkir_inspectProgram(ir, nksb_getStream(&sb));
    NK_LOG_INF("IR:\n" NKS_FMT, NKS_ARG(sb));
#endif // ENABLE_LOGGING

    // Run

    auto run_ctx = nkir_createRunCtx(ir, &m_tmp_arena);
    defer {
        nkir_freeRunCtx(run_ctx);
    };

    i64 result = -1;
    void *rets[] = {&result};

    nkir_invoke(run_ctx, proc, NULL, rets);

    EXPECT_EQ(result, 42);
}
