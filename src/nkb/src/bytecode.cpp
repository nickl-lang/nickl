#include "bytecode.hpp"

#include <cassert>

#include "ffi_adapter.hpp"
#include "interp.hpp"
#include "ir_impl.hpp"
#include "nk/common/array.h"
#include "nk/common/logger.h"
#include "nk/common/string.h"
#include "nkb/common.h"
#include "nkb/ir.h"

namespace {

NK_LOG_USE_SCOPE(bytecode);

NkBcOpcode s_ir2opcode[] = {
#define IR(NAME) CAT(nkop_, NAME),
#include "nkb/ir.inl"
};

#ifdef ENABLE_LOGGING
nkav_typedef(NkBcInstr, NkBcInstrView);

void inspect(NkBcInstrView instrs, NkStringBuilder *sb) {
    auto inspect_ref = [&](NkBcRef const &ref) {
        if (ref.kind == NkBcRef_None) {
            nksb_printf(sb, "(null)");
            return;
        } else if (ref.kind == NkBcRef_Instr) {
            nksb_printf(sb, "instr@%zi", ref.offset / sizeof(NkBcInstr));
            return;
        }
        for (size_t i = 0; i < ref.indir; i++) {
            nksb_printf(sb, "[");
        }
        switch (ref.kind) {
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
            nkirv_inspect((void *)ref.offset, ref.type, sb);
            break;
        case NkBcRef_Data:
            nksb_printf(sb, "data+");
            break;
        default:
        case NkBcRef_None:
        case NkBcRef_Instr:
            assert(!"unreachable");
            break;
        }
        if (ref.kind != NkBcRef_Rodata) {
            nksb_printf(sb, "%zx", ref.offset);
        }
        for (size_t i = 0; i < ref.indir; i++) {
            nksb_printf(sb, "]");
        }
        if (ref.post_offset) {
            nksb_printf(sb, "+%zx", ref.post_offset);
        }
        if (ref.type) {
            nksb_printf(sb, ":");
            nkirt_inspect(ref.type, sb);
        }
    };

    auto inspect_arg = [&](NkBcArg const &arg) {
        switch (arg.kind) {
        case NkBcArg_Ref: {
            inspect_ref(arg.ref);
            break;
        }

        case NkBcArg_RefArray:
            nksb_printf(sb, "(");
            for (size_t i = 0; i < arg.refs.size; i++) {
                if (i) {
                    nksb_printf(sb, ", ");
                }
                inspect_ref(arg.refs.data[i]);
            }
            nksb_printf(sb, ")");
            break;

        default:
            break;
        }
    };

    for (auto const &instr : nk_iterate(instrs)) {
        nksb_printf(sb, "%5zu", (&instr - instrs.data));
        nksb_printf(sb, "%13s", nkbcOpcodeName(instr.code));

        for (size_t i = 1; i < 3; i++) {
            if (instr.arg[i].kind != NkBcArg_None) {
                nksb_printf(sb, ((i > 1) ? ", " : " "));
                inspect_arg(instr.arg[i]);
            }
        }

        if (instr.arg[0].ref.kind != NkBcRef_None) {
            nksb_printf(sb, " -> ");
            inspect_arg(instr.arg[0]);
        }

        nksb_printf(sb, "\n");
    }
}
#endif // ENABLE_LOGGING

void translateProc(NkIrRunCtx ctx, NkIrProc proc_id) {
    while (proc_id.id >= ctx->procs.size) {
        nkar_append(&ctx->procs, nullptr);
    }

    if (ctx->procs.data[proc_id.id]) {
        return;
    }

    NK_LOG_TRC("%s", __func__);

    auto const tmp_alloc = nk_arena_getAllocator(ctx->tmp_arena);
    auto const frame = nk_arena_grab(ctx->tmp_arena);
    defer {
        nk_arena_popFrame(ctx->tmp_arena, frame);
    };

    auto const &ir = *ctx->ir;
    auto const &ir_proc = ir.procs.data[proc_id.id];

    auto const local_counts = nk_alloc_t<size_t>(tmp_alloc, ir_proc.locals.size);
    for (size_t i = 0; i < ir_proc.locals.size; i++) {
        local_counts[i] = 1lu;
    }
    auto const frame_layout =
        nkir_calcAggregateLayout(tmp_alloc, ir_proc.locals.data, local_counts, ir_proc.locals.size, 1);

    auto &bc_proc =
        *(ctx->procs.data[proc_id.id] = new (nk_alloc_t<NkBcProc_T>(ir.alloc)) NkBcProc_T{
              .ctx = ctx,
              .frame_size = frame_layout.size,
              .instrs{0, 0, 0, ir.alloc},
          });

    enum ERelocType {
        Reloc_Block,
        Reloc_Proc,
        Reloc_Closure,
    };

    struct Reloc {
        size_t instr_index;
        size_t arg_index;
        size_t ref_index;
        size_t target_id;
        ERelocType reloc_type;
        NkIrProcInfo const *proc_info{};
    };

    struct BlockInfo {
        size_t first_instr;
    };

    nkar_type(BlockInfo) block_info{0, 0, 0, tmp_alloc};
    nkar_type(Reloc) relocs{0, 0, 0, tmp_alloc};
    nkar_type(NkIrProc) referenced_procs{0, 0, 0, tmp_alloc};

    auto const get_global_addr = [&](size_t index) {
        while (index >= ctx->globals.size) {
            nkar_append(&ctx->globals, nullptr);
        }
        auto &data = ctx->globals.data[index];
        if (!data) {
            auto const type = ir.globals.data[index];
            data = nk_allocAligned(ir.alloc, type->size, type->align);
            std::memset(data, 0, type->size);
        }
        return data;
    };

    auto const translate_ref =
        [&](size_t instr_index, size_t arg_index, size_t ref_index, NkBcRef &ref, NkIrRef const &ir_ref) {
            ref = {
                .offset = ir_ref.offset,
                .post_offset = ir_ref.post_offset,
                .type = ir_ref.type,
                .kind = (NkBcRefKind)ir_ref.kind,
                .indir = ir_ref.indir,
            };

            switch (ir_ref.kind) {
            case NkIrRef_Frame:
                ref.offset += frame_layout.info_ar.data[ir_ref.index].offset;
                break;
            case NkIrRef_Arg:
                ref.offset += ir_ref.index * sizeof(void *);
                break;
            case NkIrRef_Ret:
                break;
            case NkIrRef_Data: {
                ref.offset += (size_t)get_global_addr(ir_ref.index);
                break;
            }
            case NkIrRef_Rodata: {
                ref.kind = NkBcRef_Rodata;
                auto const_data = ir.consts.data[ir_ref.index].data;
                ref.offset = (size_t)const_data;
                break;
            }
            case NkIrRef_Proc: {
                ref.kind = NkBcRef_Rodata;
                if (ir_ref.type->as.proc.info.call_conv == NkCallConv_Cdecl) {
                    nkar_append(
                        &relocs,
                        (Reloc{
                            .instr_index = instr_index,
                            .arg_index = arg_index,
                            .ref_index = ref_index,
                            .target_id = ir_ref.index,
                            .reloc_type = Reloc_Closure,
                            .proc_info = &ir_ref.type->as.proc.info,
                        }));
                } else {
                    nkar_append(
                        &relocs,
                        (Reloc{
                            .instr_index = instr_index,
                            .arg_index = arg_index,
                            .ref_index = ref_index,
                            .target_id = ir_ref.index,
                            .reloc_type = Reloc_Proc,
                        }));
                }
                nkar_append(&referenced_procs, NkIrProc{ir_ref.index});
                break;
            }
            case NkIrRef_ExternData: {
                auto data = ir.extern_data.data[ir_ref.index];

                auto found = ctx->extern_syms.find(s2nkid(data.name));
                assert(found && "extern data not found");

                ref.kind = NkBcRef_Data;
                ref.offset += (size_t)*found;
                break;
            }
            case NkIrRef_ExternProc: {
                auto proc = ir.extern_procs.data[ir_ref.index];

                auto found = ctx->extern_syms.find(s2nkid(proc.name));
                assert(found && "extern proc not found");
                auto sym_addr = nk_alloc_t<void *>(ir.alloc);
                *sym_addr = *found;

                ref.kind = NkBcRef_Rodata;
                ref.offset += (size_t)sym_addr;
                break;
            }
            case NkIrRef_Address: {
                auto const &target_ref = ir.relocs.data[ir_ref.index];
                auto ref_addr = nk_alloc_t<void *>(ir.alloc);
                switch (target_ref.kind) {
                case NkIrRef_Data:
                    *ref_addr = get_global_addr(target_ref.index);
                    break;
                case NkIrRef_Rodata:
                    *ref_addr = ir.consts.data[target_ref.index].data;
                    break;
                default:
                    assert(!"unreachable");
                    break;
                }

                ref.kind = NkBcRef_Rodata;
                ref.offset += (size_t)ref_addr;

                break;
            }
            default:
                assert(!"unreachable");
            case NkIrRef_None:
                break;
            }
        };

    auto const translate_arg = [&](size_t instr_index, size_t arg_index, NkBcArg &arg, NkIrArg const &ir_arg) {
        switch (ir_arg.kind) {
        case NkIrArg_None: {
            arg.kind = NkBcArg_None;
            break;
        }

        case NkIrArg_Ref: {
            arg.kind = NkBcArg_Ref;
            translate_ref(instr_index, arg_index, -1ul, arg.ref, ir_arg.ref);
            break;
        }

        case NkIrArg_RefArray: {
            arg.kind = NkBcArg_RefArray;
            auto refs = nk_alloc_t<NkBcRef>(ir.alloc, ir_arg.refs.size);
            for (size_t i = 0; i < ir_arg.refs.size; i++) {
                translate_ref(instr_index, arg_index, i, refs[i], ir_arg.refs.data[i]);
            }
            arg.refs = {refs, ir_arg.refs.size};
            break;
        }

        case NkIrArg_Label: {
            arg.kind = NkBcArg_Ref;
            arg.ref.kind = NkBcRef_Instr;
            nkar_append(
                &relocs,
                (Reloc{
                    .instr_index = instr_index,
                    .arg_index = arg_index,
                    .ref_index = -1ul,
                    .target_id = ir_arg.id,
                    .reloc_type = Reloc_Block,
                }));
            break;
        }
        }
    };

    for (auto block_id : nk_iterate(ir_proc.blocks)) {
        auto const &block = ir.blocks.data[block_id];

        while (block_id >= block_info.size) {
            nkar_append(&block_info, BlockInfo{});
        }
        block_info.data[block_id].first_instr = bc_proc.instrs.size;

        for (auto const &ir_instr_id : nk_iterate(block.instrs)) {
            auto const &ir_instr = ir.instrs.data[ir_instr_id];

            uint16_t code = s_ir2opcode[ir_instr.code];

            auto const &arg0 = ir_instr.arg[0];
            auto const &arg1 = ir_instr.arg[1];

            switch (ir_instr.code) {
            case nkir_call: {
                auto const &arg1 = ir_instr.arg[1];
                if (arg1.ref.kind == NkIrRef_ExternProc ||
                    (arg1.ref.kind == NkIrRef_Proc && arg1.ref.type->as.proc.info.call_conv != NkCallConv_Nk)) {
                    code = nkop_call_ext;
                } else if (arg1.ref.kind == NkIrRef_Proc) {
                    code = nkop_call_jmp;
                }

                break;
            }

#define SIZ_OP(NAME) case CAT(nkir_, NAME):
#include "bytecode.inl"
                {
                    assert(arg0.kind == NkIrArg_Ref || arg1.kind == NkIrArg_Ref);
                    auto const ref_type = arg0.kind == NkIrArg_Ref ? arg0.ref.type : arg1.ref.type;
                    if (ref_type->size <= 8 && isZeroOrPowerOf2(ref_type->size)) {
                        code += 1 + log2u64(ref_type->size);
                    }
                    break;
                }

#define BOOL_NUM_OP(NAME)
#define NUM_OP(NAME) case CAT(nkir_, NAME):
#define INT_OP(NAME) case CAT(nkir_, NAME):
#include "bytecode.inl"
                assert(arg0.ref.type->kind == NkType_Numeric);
                code += 1 + NKIR_NUMERIC_TYPE_INDEX(arg0.ref.type->as.num.value_type);
                break;

#define BOOL_NUM_OP(NAME) case CAT(nkir_, NAME):
#include "bytecode.inl"
                assert(arg1.ref.type->kind == NkType_Numeric);
                code += 1 + NKIR_NUMERIC_TYPE_INDEX(arg1.ref.type->as.num.value_type);
                break;
            }

            nkar_append(&bc_proc.instrs, NkBcInstr{});
            auto &instr = nkar_last(bc_proc.instrs);
            instr.code = code;
            for (size_t ai = 0; ai < 3; ai++) {
                translate_arg(bc_proc.instrs.size - 1, ai, instr.arg[ai], ir_instr.arg[ai]);
            }
        }
    }

    for (auto proc : nk_iterate(referenced_procs)) {
        translateProc(ctx, proc);
    }

    for (auto const &reloc : nk_iterate(relocs)) {
        auto &arg = bc_proc.instrs.data[reloc.instr_index].arg[reloc.arg_index];
        auto &ref = reloc.ref_index == -1ul ? arg.ref : arg.refs.data[reloc.ref_index];

        switch (reloc.reloc_type) {
        case Reloc_Block: {
            ref.offset = block_info.data[reloc.target_id].first_instr * sizeof(NkBcInstr);
            break;
        }
        case Reloc_Proc: {
            auto sym_addr = nk_alloc_t<void *>(ir.alloc);
            *sym_addr = ctx->procs.data[reloc.target_id];
            ref.offset = (size_t)sym_addr;
            break;
        }
        case Reloc_Closure: {
            NkNativeCallData const call_data{
                .proc{.bytecode = ctx->procs.data[reloc.target_id]},
                .nfixedargs = reloc.proc_info->args_t.size,
                .is_variadic = (bool)(reloc.proc_info->flags & NkProcVariadic),
                .argv{},
                .argt = reloc.proc_info->args_t.data,
                .argc = reloc.proc_info->args_t.size,
                .retv{},
                .rett = reloc.proc_info->ret_t.data[0],
            };
            ref.offset = (size_t)nk_native_makeClosure(&ctx->ffi_ctx, ctx->tmp_arena, ir.alloc, &call_data);
            break;
        }
        }
    }

#ifdef ENABLE_LOGGING
    NkStringBuilder sb{};
    sb.alloc = tmp_alloc;
    inspect({bc_proc.instrs.data, bc_proc.instrs.size}, &sb);
    NK_LOG_INF("proc " nkstr_Fmt "\n" nkstr_Fmt "", nkstr_Arg(ir_proc.name), nkstr_Arg(sb));
#endif // ENABLE_LOGGING
}

} // namespace

char const *nkbcOpcodeName(uint16_t code) {
    switch (code) {
#define OP(NAME)           \
    case CAT(nkop_, NAME): \
        return #NAME;
#define OPX(NAME, EXT)                       \
    case CAT(nkop_, CAT(NAME, CAT(_, EXT))): \
        return #NAME "(" #EXT ")";
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

        .procs{0, 0, 0, ir->alloc},
        .globals{0, 0, 0, ir->alloc},
        .extern_syms = decltype(NkIrRunCtx_T::extern_syms)::create(ir->alloc),

        .ffi_ctx{
            .alloc = ir->alloc,
        },
    };
}

void nkir_freeRunCtx(NkIrRunCtx ctx) {
    NK_LOG_TRC("%s", __func__);

    ctx->ffi_ctx.typemap.deinit();

    nk_free_t(ctx->ir->alloc, ctx);
}

void nkir_defineExternSym(NkIrRunCtx ctx, nkstr name, void *data) {
    NK_LOG_TRC("%s", __func__);

    ctx->extern_syms.insert(s2nkid(name), data);
}

void nkir_invoke(NkIrRunCtx ctx, NkIrProc proc_id, void **args, void **ret) {
    NK_LOG_TRC("%s", __func__);

    translateProc(ctx, proc_id);
    nkir_interp_invoke(ctx->procs.data[proc_id.id], args, ret);
}
