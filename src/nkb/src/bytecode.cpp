#include "bytecode.hpp"

#include <cassert>

#include "interp.hpp"
#include "ir_impl.hpp"
#include "nk/common/logger.h"

namespace {

NK_LOG_USE_SCOPE(bytecode);

NkBcOpcode s_ir2opcode[] = {
#define IR(NAME) CAT(nkop_, NAME),
#include "nkb/ir.inl"
};

void inspect(NkSlice<NkBcInstr> instrs, NkStringBuilder sb) {
    auto inspect_arg = [&](NkBcRef const &arg) {
        if (arg.is_indirect) {
            nksb_printf(sb, "[");
        }
        switch (arg.kind) {
        case NkBcRef_Frame:
            nksb_printf(sb, "frame+");
            break;
        case NkBcRef_Arg:
            nksb_printf(sb, "arg+");
            break;
        case NkBcRef_Ret:
            nksb_printf(sb, "ret+");
            break;
        case NkBcRef_Rodata:
            nkirv_inspect((void *)arg.offset, arg.type, sb);
            break;
        case NkBcRef_Data:
            nksb_printf(sb, "data+");
            break;
        case NkBcRef_Instr:
            nksb_printf(sb, "instr+");
            break;
        default:
            assert(!"unreachable");
            break;
        }
        if (arg.is_indirect) {
            nksb_printf(sb, "]");
        }
        if (arg.kind != NkBcRef_Rodata) {
            nksb_printf(sb, "%zx", arg.offset);
        }
        if (arg.type) {
            nksb_printf(sb, ":");
            nkirt_inspect(arg.type, sb);
        }
    };

    for (auto const &instr : instrs) {
        nksb_printf(sb, "%zx\t", (&instr - instrs.data()) * sizeof(NkBcInstr));

        nksb_printf(sb, "%s", nkbcOpcodeName(instr.code));

        for (size_t i = 1; i < 3; i++) {
            if (instr.arg[i].kind != NkBcRef_None) {
                nksb_printf(sb, ((i > 1) ? ", " : " "));
                inspect_arg(instr.arg[i]);
            }
        }

        if (instr.arg[0].kind != NkBcRef_None) {
            nksb_printf(sb, " -> ");
            inspect_arg(instr.arg[0]);
        }

        nksb_printf(sb, "\n");
    }
}

void translateProc(NkIrRunCtx ctx, NkIrProc proc_id) {
    while (proc_id.id >= ctx->procs.size()) {
        ctx->procs.emplace(nullptr);
    }

    if (ctx->procs[proc_id.id]) {
        return;
    }

    NK_LOG_TRC("%s", __func__);

    auto const tmp_alloc = nk_arena_getAllocator(ctx->tmp_arena);
    auto const frame = nk_arena_grab(ctx->tmp_arena);
    defer {
        nk_arena_popFrame(ctx->tmp_arena, frame);
    };

    auto const &ir = *ctx->ir;
    auto const &ir_proc = ir.procs[proc_id.id];

    auto const local_counts = nk_alloc_t<size_t>(tmp_alloc, ir_proc.locals.size());
    for (size_t i = 0; i < ir_proc.locals.size(); i++) {
        local_counts[i] = 1lu;
    }
    auto const frame_layout =
        nkir_calcAggregateLayout(tmp_alloc, ir_proc.locals.data(), local_counts, ir_proc.locals.size(), 1);

    auto &bc_proc =
        *(ctx->procs[proc_id.id] = new (nk_alloc_t<NkBcProc_T>(ir.alloc)) NkBcProc_T{
              .ctx = ctx,
              .frame_size = frame_layout.size,
              .instrs = decltype(NkBcProc_T::instrs)::create(ir.alloc),
          });

    enum ERelocType {
        Reloc_Block,
    };

    struct Reloc {
        size_t instr_index;
        size_t arg;
        size_t target_id;
        ERelocType reloc_type;
    };

    struct BlockInfo {
        size_t first_instr;
    };

    auto block_info = NkArray<BlockInfo>::create(tmp_alloc);
    auto relocs = NkArray<Reloc>::create(tmp_alloc);

    auto referenced_procs = NkArray<NkIrProc>::create(tmp_alloc);

    auto translate_arg = [&](size_t ii, size_t ai, NkBcRef &arg, NkIrArg const &ir_arg) { // TODO
        switch (ir_arg.arg_kind) {
        case NkIrArg_None:
            arg.kind = NkBcRef_None;
            break;

        case NkIrArg_Ref: {
            auto const &ref = ir_arg.ref;
            arg.offset += ref.offset;
            arg.type = ref.type;
            arg.kind = (NkBcRefKind)ref.kind;
            arg.is_indirect = ref.is_indirect;
            switch (ref.kind) {
            case NkIrRef_Frame:
                arg.offset += frame_layout.info_ar.data[ref.index].offset;
                break;
            case NkIrRef_Arg:
                arg.kind = NkBcRef_None;
                NK_LOG_WRN("TODO NkIrRef_Arg translation not implemented");
                break;
            case NkIrRef_Ret:
                break;
            case NkIrRef_Data: {
                arg.kind = NkBcRef_None;
                NK_LOG_WRN("TODO NkIrRef_Data translation not implemented");
                break;
            }
            case NkIrRef_Rodata: {
                arg.kind = NkBcRef_Rodata;
                auto const_data = ir.consts[ref.index].data;
                arg.offset = (size_t)const_data;
                break;
            }
            case NkIrRef_Proc: {
                arg.kind = NkBcRef_None;
                NK_LOG_WRN("TODO NkIrRef_Proc translation not implemented");
                break;
            };
            case NkIrRef_ExternData: {
                arg.kind = NkBcRef_None;
                NK_LOG_WRN("TODO NkIrRef_ExternData translation not implemented");
                break;
            }
            case NkIrRef_ExternProc: {
                arg.kind = NkBcRef_None;
                NK_LOG_WRN("TODO NkIrRef_ExternData translation not implemented");
                break;
            }
            case NkIrRef_Reloc: {
                arg.kind = NkBcRef_None;
                NK_LOG_WRN("TODO NkIrRef_Reloc translation not implemented");
                break;
            }
            default:
                assert(!"unreachable");
            case NkIrRef_None:
                break;
            }
            break;
        }

        case NkIrArg_RefArray:
            NK_LOG_WRN("TODO NkIrArg_RefArray translation not implemented");
            break;

        case NkIrArg_Label:
            arg.kind = NkBcRef_Instr;
            relocs.emplace(Reloc{
                .instr_index = ii,
                .arg = ai,
                .target_id = ir_arg.id,
                .reloc_type = Reloc_Block,
            });
            break;
        }
    };

    for (auto block_id : ir_proc.blocks) {
        auto const &block = ir.blocks[block_id];

        while (block_id >= block_info.size()) {
            block_info.emplace();
        }
        block_info[block_id].first_instr = bc_proc.instrs.size();

        for (auto const &ir_instr_id : block.instrs) {
            auto const &ir_instr = ir.instrs[ir_instr_id];

            uint16_t code = s_ir2opcode[ir_instr.code];

            auto const &arg1 = ir_instr.arg[1];

            switch (ir_instr.code) {
            case nkir_call: {
                NK_LOG_WRN("TODO nkir_call translation not implemented");
                break;
            }

#define SIZ_OP(NAME) case CAT(nkir_, NAME):
#include "bytecode.inl"
                if (arg1.ref.type->size <= 8 && isZeroOrPowerOf2(arg1.ref.type->size)) {
                    code += 1 + log2u64(arg1.ref.type->size);
                }
                break;

#define NUM_OP(NAME) case CAT(nkir_, NAME):
#define INT_OP(NAME) case CAT(nkir_, NAME):
#include "bytecode.inl"
                assert(arg1.ref.type->kind == NkType_Basic);
                code += 1 + NKIR_BASIC_TYPE_INDEX(arg1.ref.type->as.basic.value_type);
                break;

            default:
                assert(!"unreachable");
                break;
            }

            auto &instr = bc_proc.instrs.emplace();
            instr.code = code;
            for (size_t ai = 0; ai < 3; ai++) {
                translate_arg(bc_proc.instrs.size() - 1, ai, instr.arg[ai], ir_instr.arg[ai]);
            }
        }
    }

    for (auto const &reloc : relocs) {
        NkBcRef &arg = bc_proc.instrs[reloc.instr_index].arg[reloc.arg];

        switch (reloc.reloc_type) {
        case Reloc_Block:
            arg.offset = block_info[reloc.target_id].first_instr * sizeof(NkBcInstr);
            break;
        }
    }

#ifdef ENABLE_LOGGING
    NkStringBuilder_T sb{};
    nksb_init_alloc(&sb, tmp_alloc);
    inspect(bc_proc.instrs, &sb);
    auto ir_str = nksb_concat(&sb);
    NK_LOG_INF("Bytecode:\n\n%.*s", (int)ir_str.size, ir_str.data);
#endif // ENABLE_LOGGING

    for (auto proc : referenced_procs) {
        translateProc(ctx, proc);
    }
}

} // namespace

char const *nkbcOpcodeName(uint16_t code) {
    switch (code) {
#define OP(NAME)           \
    case CAT(nkop_, NAME): \
        return #NAME;
#define OPX(NAME, EXT)                       \
    case CAT(nkop_, CAT(NAME, CAT(_, EXT))): \
        return #NAME " (" #EXT ")";
#include "bytecode.inl"

    default:
        return "";
    }
}

NkIrRunCtx nkir_createRunCtx(NkIrProg ir, NkArena *tmp_arena) {
    NK_LOG_TRC("%s", __func__);

    return new (nk_alloc_t<NkIrRunCtx_T>(ir->alloc)) NkIrRunCtx_T{
        .ir = ir,
        .tmp_arena = tmp_arena,

        .procs = decltype(NkIrRunCtx_T::procs)::create(ir->alloc),
        .extern_syms = decltype(NkIrRunCtx_T::extern_syms)::create(ir->alloc),
    };
}

void nkir_freeRunCtx(NkIrRunCtx ctx) {
    NK_LOG_TRC("%s", __func__);

    nk_free_t(ctx->ir->alloc, ctx);
}

void nkir_defineExternSym(NkIrRunCtx ctx, nkstr name, void *data) {
    NK_LOG_TRC("%s", __func__);

    ctx->extern_syms.insert(s2nkid(name), data);
}

void nkir_invoke(NkIrRunCtx ctx, NkIrProc proc_id, NkIrPtrArray args, NkIrPtrArray ret) {
    NK_LOG_TRC("%s", __func__);

    translateProc(ctx, proc_id);
    nkir_interp_invoke(ctx->procs[proc_id.id], args, ret);
}
