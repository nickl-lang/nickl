#include <cstdint>

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
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(test);

class ir_paste : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});

        buildIr();
    }

    void TearDown() override {
        nkir_freeRunCtx(run_ctx);

        nk_arena_free(&m_tmp_arena);
        nk_arena_free(&m_arena);
    }

protected:
    void buildIr() {
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

        ir = nkir_createProgram(&m_arena);

        auto const test_shouldFreeResources_ref = nkir_makeExternProcRef(
            ir, nkir_makeExternProc(ir, nk_cs2atom(""), nk_cs2atom("test_shouldFreeResources"), &proc_t));
        auto const test_freeResources_ref = nkir_makeExternProcRef(
            ir, nkir_makeExternProc(ir, nk_cs2atom(""), nk_cs2atom("test_freeResources"), &proc_t));
        auto const test_doStuff_ref =
            nkir_makeExternProcRef(ir, nkir_makeExternProc(ir, nk_cs2atom(""), nk_cs2atom("test_doStuff"), &proc_t));
        auto const test_doMoreStuff_ref = nkir_makeExternProcRef(
            ir, nkir_makeExternProc(ir, nk_cs2atom(""), nk_cs2atom("test_doMoreStuff"), &proc_t));

        auto const proc = nkir_createProc(ir);
        nkir_startProc(ir, proc, nk_cs2atom("foo"), &proc_t, {}, NK_ATOM_INVALID, 0, NkIrVisibility_Default);

        nkir_emit(ir, nkir_make_label(nkir_createLabel(ir, nk_cs2atom("@start"))));

        // foo :: () i64 {
        //     defer {
        //         if test_shouldFreeResources() != 0 {
        //             test_freeResources();
        //         }
        //     }
        //     if test_doStuff() == 0 {
        //         return 0;
        //     }
        //     return test_doMoreStuff();
        // }

        NkDynArray(NkIrInstr) defer_instrs{NKDA_INIT(nk_arena_getAllocator(&m_tmp_arena))};

        auto endif_defer_l = nkir_createLabel(ir, nk_cs2atom("@endif"));
        auto const should_free_ref = nkir_makeFrameRef(ir, nkir_makeLocalVar(ir, nk_cs2atom("should_free"), &i64_t));
        // nkda_append(&defer_instrs, nkir_make_comment(ir, nk_cs2s("defer start")));
        nkda_append(&defer_instrs, nkir_make_call(ir, should_free_ref, test_shouldFreeResources_ref, {}));
        nkda_append(&defer_instrs, nkir_make_jmpz(ir, should_free_ref, endif_defer_l));
        nkda_append(&defer_instrs, nkir_make_call(ir, should_free_ref, test_freeResources_ref, {}));
        nkda_append(&defer_instrs, nkir_make_label(endif_defer_l));
        // nkda_append(&defer_instrs, nkir_make_comment(ir, nk_cs2s("defer end")));

        auto endif_l = nkir_createLabel(ir, nk_cs2atom("@endif"));
        auto const should_do_more_stuff_ref =
            nkir_makeFrameRef(ir, nkir_makeLocalVar(ir, nk_cs2atom("should_do_more_stuff"), &i64_t));
        nkir_emit(ir, nkir_make_call(ir, should_do_more_stuff_ref, test_doStuff_ref, {}));
        nkir_emit(ir, nkir_make_jmpnz(ir, should_do_more_stuff_ref, endif_l));
        nkir_emit(ir, nkir_make_call(ir, should_do_more_stuff_ref, test_freeResources_ref, {}));
        {
            auto const cnst = nkir_makeRodata(ir, NK_ATOM_INVALID, &i64_t, NkIrVisibility_Local);
            *(i64 *)nkir_getDataPtr(ir, cnst) = 0;
            nkir_emit(ir, nkir_make_mov(ir, nkir_makeRetRef(ir), nkir_makeDataRef(ir, cnst)));

            nkir_paste(ir, {NK_SLICE_INIT(defer_instrs)});
            nkir_emit(ir, nkir_make_ret(ir));
        }
        nkir_emit(ir, nkir_make_label(endif_l));

        nkir_emit(ir, nkir_make_call(ir, nkir_makeRetRef(ir), test_doMoreStuff_ref, {}));
        nkir_paste(ir, {NK_SLICE_INIT(defer_instrs)});
        nkir_emit(ir, nkir_make_ret(ir));

        nkir_finishProc(ir, proc, 0);

#ifdef ENABLE_LOGGING
        NkStringBuilder sb{};
        sb.alloc = nk_arena_getAllocator(&m_tmp_arena);
        nkir_inspectProgram(ir, nksb_getStream(&sb));
        NK_LOG_INF("IR:\n" NKS_FMT, NKS_ARG(sb));
#endif // ENABLE_LOGGING

        run_ctx = nkir_createRunCtx(ir, &m_tmp_arena);
    }

    i64 invokeProc() {
        i64 result = -1;
        void *rets[] = {&result};

        if (!nkir_invoke(run_ctx, proc, NULL, rets)) {
            auto const msg = nkir_getRunErrorString(run_ctx);
            NK_LOG_ERR(NKS_FMT, NKS_ARG(msg));
        }

        return result;
    }

protected:
    NkArena m_arena{};
    NkArena m_tmp_arena{};

    NkIrProg ir;
    NkIrProc proc;
    NkIrRunCtx run_ctx;
};

} // namespace

static i64 g_shouldFreeResources_result = 0;
extern "C" NK_EXPORT i64 test_shouldFreeResources() {
    return 0;
}

static i64 g_freeResources_result = 0;
extern "C" NK_EXPORT i64 test_freeResources() {
    return 0;
}

static i64 g_doStuff_result = 0;
extern "C" NK_EXPORT i64 test_doStuff() {
    return 0;
}

static i64 g_doMoreStuff_result = 0;
extern "C" NK_EXPORT i64 test_doMoreStuff() {
    return 0;
}

TEST_F(ir_paste, defer_scenario) {
}
