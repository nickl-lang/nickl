#include <gtest/gtest.h>

#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/slice.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

namespace {

NK_LOG_USE_SCOPE(test);

static i64 g_shouldFreeResources_result = 0;
static i64 g_shouldFreeResources_invoke_times = 0;
extern "C" NK_EXPORT i64 test_shouldFreeResources() {
    g_shouldFreeResources_invoke_times++;
    return g_shouldFreeResources_result;
}

static i64 g_freeResources_result = 0;
static i64 g_freeResources_invoke_times = 0;
extern "C" NK_EXPORT i64 test_freeResources() {
    g_freeResources_invoke_times++;
    return g_freeResources_result;
}

static i64 g_doStuff_result = 0;
static i64 g_doStuff_invoke_times = 0;
extern "C" NK_EXPORT i64 test_doStuff() {
    g_doStuff_invoke_times++;
    return g_doStuff_result;
}

static i64 g_doMoreStuff_result = 0;
static i64 g_doMoreStuff_invoke_times = 0;
extern "C" NK_EXPORT i64 test_doMoreStuff() {
    g_doMoreStuff_invoke_times++;
    return g_doMoreStuff_result;
}

class ir_paste : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});

        buildIr();
        resetGlobals();
    }

    void TearDown() override {
        nkir_freeRunCtx(m_run_ctx);

        nk_arena_free(&m_tmp_arena);
        nk_arena_free(&m_arena);
    }

protected:
    static void resetGlobals() {
        g_shouldFreeResources_result = 0;
        g_freeResources_result = 0;
        g_doStuff_result = 0;
        g_doMoreStuff_result = 0;
        g_shouldFreeResources_invoke_times = 0;
        g_freeResources_invoke_times = 0;
        g_doStuff_invoke_times = 0;
        g_doMoreStuff_invoke_times = 0;
    }

    void buildIr() {
        u32 type_id = 0;

        m_i64_t = {
            .as{.num{Int64}},
            .size = 8,
            .flags = 0,
            .align = 8,
            .kind = NkIrType_Numeric,
            .id = type_id++,
        };

        m_proc_t = {
            .as{.proc{{
                .args_t{},
                .ret_t = &m_i64_t,
                .call_conv = NkCallConv_Nk,
                .flags = 0,
            }}},
            .size = 8,
            .flags = 0,
            .align = 8,
            .kind = NkIrType_Procedure,
            .id = type_id++,
        };

        m_ir = nkir_createProgram(&m_arena);

        auto const test_shouldFreeResources_ref = nkir_makeExternProcRef(
            m_ir, nkir_makeExternProc(m_ir, nk_cs2atom(""), nk_cs2atom("test_shouldFreeResources"), &m_proc_t));
        auto const test_freeResources_ref = nkir_makeExternProcRef(
            m_ir, nkir_makeExternProc(m_ir, nk_cs2atom(""), nk_cs2atom("test_freeResources"), &m_proc_t));
        auto const test_doStuff_ref = nkir_makeExternProcRef(
            m_ir, nkir_makeExternProc(m_ir, nk_cs2atom(""), nk_cs2atom("test_doStuff"), &m_proc_t));
        auto const test_doMoreStuff_ref = nkir_makeExternProcRef(
            m_ir, nkir_makeExternProc(m_ir, nk_cs2atom(""), nk_cs2atom("test_doMoreStuff"), &m_proc_t));

        m_proc = nkir_createProc(m_ir);

        auto const forty_two = nkir_makeRodata(m_ir, NK_ATOM_INVALID, &m_i64_t, NkIrVisibility_Local);
        *(i64 *)nkir_getDataPtr(m_ir, forty_two) = 42;

        auto const nine = nkir_makeRodata(m_ir, NK_ATOM_INVALID, &m_i64_t, NkIrVisibility_Local);
        *(i64 *)nkir_getDataPtr(m_ir, nine) = 9;

        auto const do_stuff_res_ref =
            nkir_makeFrameRef(m_ir, nkir_makeLocalVar(m_ir, nk_cs2atom("do_stuff_res"), &m_i64_t));
        auto const should_free_ref =
            nkir_makeFrameRef(m_ir, nkir_makeLocalVar(m_ir, nk_cs2atom("should_free"), &m_i64_t));

        auto const endif_defer_l = nkir_createLabel(m_ir, nk_cs2atom("@endif_defer"));
        auto const endif_l = nkir_createLabel(m_ir, nk_cs2atom("@endif"));

        // proc foo() i64 {
        nkir_startProc(m_ir, m_proc, nk_cs2atom("foo"), &m_proc_t, {}, NK_ATOM_INVALID, 0, NkIrVisibility_Default);
        nkir_emit(m_ir, nkir_make_label(nkir_createLabel(m_ir, nk_cs2atom("@start"))));
        //     {
        //         defer {
        //             if test_shouldFreeResources() != 0 {
        NkDynArray(NkIrInstr) defer_instrs{NKDA_INIT(nk_arena_getAllocator(&m_tmp_arena))};
        nkda_append(&defer_instrs, nkir_make_comment(m_ir, nk_cs2s("defer start")));
        nkda_append(&defer_instrs, nkir_make_call(m_ir, should_free_ref, test_shouldFreeResources_ref, {}));
        nkda_append(&defer_instrs, nkir_make_jmpz(m_ir, should_free_ref, endif_defer_l));
        //                 test_freeResources();
        nkda_append(&defer_instrs, nkir_make_call(m_ir, {}, test_freeResources_ref, {}));
        //             }
        //         }
        nkda_append(&defer_instrs, nkir_make_label(endif_defer_l));
        nkda_append(&defer_instrs, nkir_make_comment(m_ir, nk_cs2s("defer end")));
        //         if test_doStuff() == 0 {
        nkir_emit(m_ir, nkir_make_call(m_ir, do_stuff_res_ref, test_doStuff_ref, {}));
        nkir_emit(m_ir, nkir_make_jmpnz(m_ir, do_stuff_res_ref, endif_l));
        //              return 42;
        nkir_emit(m_ir, nkir_make_mov(m_ir, nkir_makeRetRef(m_ir), nkir_makeDataRef(m_ir, forty_two)));
        nkir_paste(m_ir, {NK_SLICE_INIT(defer_instrs)}, &m_tmp_arena);
        nkir_emit(m_ir, nkir_make_ret(m_ir));
        //         }
        nkir_emit(m_ir, nkir_make_label(endif_l));
        //         test_doMoreStuff();
        //     }
        nkir_emit(m_ir, nkir_make_call(m_ir, {}, test_doMoreStuff_ref, {}));
        nkir_paste(m_ir, {NK_SLICE_INIT(defer_instrs)}, &m_tmp_arena);
        //     return 9;
        nkir_emit(m_ir, nkir_make_mov(m_ir, nkir_makeRetRef(m_ir), nkir_makeDataRef(m_ir, nine)));
        nkir_emit(m_ir, nkir_make_ret(m_ir));
        // }
        nkir_finishProc(m_ir, m_proc, 0);

#ifdef ENABLE_LOGGING
        NkStringBuilder sb{};
        sb.alloc = nk_arena_getAllocator(&m_tmp_arena);
        nkir_inspectProgram(m_ir, nksb_getStream(&sb));
        NK_LOG_INF("IR:\n" NKS_FMT, NKS_ARG(sb));
#endif // ENABLE_LOGGING

        m_run_ctx = nkir_createRunCtx(m_ir, &m_tmp_arena);
    }

    i64 invokeProc() {
        i64 result = -1;
        void *rets[] = {&result};

        if (!nkir_invoke(m_run_ctx, m_proc, NULL, rets)) {
            auto const msg = nkir_getRunErrorString(m_run_ctx);
            NK_LOG_ERR(NKS_FMT, NKS_ARG(msg));
        }

        return result;
    }

protected:
    NkArena m_arena{};
    NkArena m_tmp_arena{};

    NkIrType m_i64_t;
    NkIrType m_proc_t;

    NkIrProg m_ir;
    NkIrProc m_proc;
    NkIrRunCtx m_run_ctx;
};

} // namespace

TEST_F(ir_paste, early_return_no_free) {
    auto const result = invokeProc();

    EXPECT_EQ(g_shouldFreeResources_invoke_times, 1);
    EXPECT_EQ(g_doStuff_invoke_times, 1);
    EXPECT_EQ(g_freeResources_invoke_times, 0);
    EXPECT_EQ(g_doMoreStuff_invoke_times, 0);

    EXPECT_EQ(result, 42);
}

TEST_F(ir_paste, early_return_free) {
    g_shouldFreeResources_result = 1;

    auto const result = invokeProc();

    EXPECT_EQ(g_shouldFreeResources_invoke_times, 1);
    EXPECT_EQ(g_doStuff_invoke_times, 1);
    EXPECT_EQ(g_freeResources_invoke_times, 1);
    EXPECT_EQ(g_doMoreStuff_invoke_times, 0);

    EXPECT_EQ(result, 42);
}

TEST_F(ir_paste, late_return_no_free) {
    g_doStuff_result = 1;

    auto const result = invokeProc();

    EXPECT_EQ(g_shouldFreeResources_invoke_times, 1);
    EXPECT_EQ(g_doStuff_invoke_times, 1);
    EXPECT_EQ(g_freeResources_invoke_times, 0);
    EXPECT_EQ(g_doMoreStuff_invoke_times, 1);

    EXPECT_EQ(result, 9);
}

TEST_F(ir_paste, late_return_free) {
    g_doStuff_result = 1;
    g_shouldFreeResources_result = 1;

    auto const result = invokeProc();

    EXPECT_EQ(g_shouldFreeResources_invoke_times, 1);
    EXPECT_EQ(g_doStuff_invoke_times, 1);
    EXPECT_EQ(g_freeResources_invoke_times, 1);
    EXPECT_EQ(g_doMoreStuff_invoke_times, 1);

    EXPECT_EQ(result, 9);
}
