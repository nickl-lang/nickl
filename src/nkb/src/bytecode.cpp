#include "bytecode.hpp"

#include <cassert>

#include "interp.hpp"
#include "ir_impl.hpp"
#include "nk/common/logger.h"
#include "nkb/ir.h"

namespace {

NK_LOG_USE_SCOPE(bytecode);

NkBcOpcode s_ir2opcode[] = {
#define IR(NAME) CAT(nkop_, NAME),
#include "nkb/ir.inl"
};

void inspect(NkSlice<NkBcInstr> instrs, NkStringBuilder sb) {
    auto inspect_ref = [&](NkBcRef const &ref) {
        if (ref.kind == NkBcRef_None) {
            nksb_printf(sb, "(null)");
            return;
        } else if (ref.kind == NkBcRef_Instr) {
            nksb_printf(sb, "instr@%zi", ref.offset / sizeof(NkBcInstr));
            return;
        }
        if (ref.is_indirect) {
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
        if (ref.is_indirect) {
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

    for (auto const &instr : instrs) {
        nksb_printf(sb, "%5zu", (&instr - instrs.data()));
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
        Reloc_Proc,
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

    std::function<void(size_t, size_t, NkBcRef &, NkIrRef const &)> translate_ref =
        [&](size_t ii, size_t ai, NkBcRef &ref, NkIrRef const &ir_ref) {
            ref = {
                .offset = ir_ref.offset,
                .post_offset = ir_ref.post_offset,
                .type = ir_ref.type,
                .kind = (NkBcRefKind)ir_ref.kind,
                .is_indirect = ir_ref.is_indirect,
            };

            switch (ir_ref.kind) {
            case NkIrRef_Frame:
                ref.offset += frame_layout.info_ar.data[ir_ref.index].offset;
                break;
            case NkIrRef_Arg:
                ref.offset += ir_ref.index * sizeof(void *); // TODO Use sizeof(void*) or ir.size_type->size ?
                break;
            case NkIrRef_Ret:
                break;
            case NkIrRef_Data: {
                ref.kind = NkBcRef_None;
                NK_LOG_WRN("TODO NkIrRef_Data translation not implemented");
                break;
            }
            case NkIrRef_Rodata: {
                ref.kind = NkBcRef_Rodata;
                auto const_data = ir.consts[ir_ref.index].data;
                ref.offset = (size_t)const_data;
                break;
            }
            case NkIrRef_Proc: {
                ref.kind = NkBcRef_Rodata;
                relocs.emplace(Reloc{
                    .instr_index = ii,
                    .arg = ai,
                    .target_id = ir_ref.index,
                    .reloc_type = Reloc_Proc,
                });
                break;
            };
            case NkIrRef_ExternData: {
                ref.kind = NkBcRef_None;
                NK_LOG_WRN("TODO NkIrRef_ExternData translation not implemented");
                break;
            }
            case NkIrRef_ExternProc: {
                auto proc = ir.extern_procs[ir_ref.index];

                auto found = ctx->extern_syms.find(s2nkid(proc.name));
                assert(found && "extern proc not found");
                auto sym_addr = nk_alloc_t<void *>(ir.alloc);
                *sym_addr = *found;

                ref.kind = NkBcRef_Rodata;
                ref.offset += (size_t)sym_addr;
                break;
            }
            case NkIrRef_Reloc: {
                auto const &target_ref = ir.relocs[ir_ref.index];
                switch (target_ref.kind) {
                case NkIrRef_Data:
                    NK_LOG_WRN("TODO NkIrRef_Data reloc translation not implemented");
                    break;

                case NkIrRef_Rodata: {
                    auto ref_addr = nk_alloc_t<void *>(ir.alloc);
                    *ref_addr = ir.consts[target_ref.index].data;

                    ref.kind = NkBcRef_Rodata;
                    ref.offset += (size_t)ref_addr;
                    break;
                }

                default:
                    assert(!"unreachable");
                    break;
                }
                break;
            }
            default:
                assert(!"unreachable");
            case NkIrRef_None:
                break;
            }
        };

    auto translate_arg = [&](size_t ii, size_t ai, NkBcArg &arg, NkIrArg const &ir_arg) {
        switch (ir_arg.kind) {
        case NkIrArg_None: {
            arg.kind = NkBcArg_None;
            break;
        }

        case NkIrArg_Ref: {
            arg.kind = NkBcArg_Ref;
            translate_ref(ii, ai, arg.ref, ir_arg.ref);
            break;
        }

        case NkIrArg_RefArray: {
            arg.kind = NkBcArg_RefArray;
            auto refs = nk_alloc_t<NkBcRef>(ir.alloc, ir_arg.refs.size);
            for (size_t i = 0; i < ir_arg.refs.size; i++) {
                translate_ref(ii, ai, refs[i], ir_arg.refs.data[i]);
            }
            arg.refs = {refs, ir_arg.refs.size};
            break;
        }

        case NkIrArg_Label: {
            arg.kind = NkBcArg_Ref;
            arg.ref.kind = NkBcRef_Instr;
            relocs.emplace(Reloc{
                .instr_index = ii,
                .arg = ai,
                .target_id = ir_arg.id,
                .reloc_type = Reloc_Block,
            });
            break;
        }
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

            auto const &arg0 = ir_instr.arg[0];
            auto const &arg1 = ir_instr.arg[1];

            switch (ir_instr.code) {
            case nkir_call: {
                auto const &arg1 = ir_instr.arg[1];
                if (arg1.ref.kind == NkIrRef_Proc) {
                    code = nkop_call_jmp;
                    referenced_procs.emplace(NkIrProc{arg1.ref.index});
                } else if (arg1.ref.kind == NkIrRef_ExternProc) {
                    code = nkop_call_ext;
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

#define NUM_OP(NAME) case CAT(nkir_, NAME):
#define INT_OP(NAME) case CAT(nkir_, NAME):
#include "bytecode.inl"
                assert(arg0.ref.type->kind == NkType_Basic);
                code += 1 + NKIR_BASIC_TYPE_INDEX(arg0.ref.type->as.basic.value_type);
                break;
            }

            auto &instr = bc_proc.instrs.emplace();
            instr.code = code;
            for (size_t ai = 0; ai < 3; ai++) {
                translate_arg(bc_proc.instrs.size() - 1, ai, instr.arg[ai], ir_instr.arg[ai]);
            }
        }
    }

    for (auto proc : referenced_procs) {
        translateProc(ctx, proc);
    }

    for (auto const &reloc : relocs) {
        auto &ref = bc_proc.instrs[reloc.instr_index].arg[reloc.arg].ref;

        switch (reloc.reloc_type) {
        case Reloc_Block: {
            ref.offset = block_info[reloc.target_id].first_instr * sizeof(NkBcInstr);
            break;
        }
        case Reloc_Proc: {
            auto sym_addr = nk_alloc_t<void *>(ir.alloc);
            *sym_addr = ctx->procs[reloc.target_id];
            ref.offset = (size_t)sym_addr;
            break;
        }
        }
    }

#ifdef ENABLE_LOGGING
    NkStringBuilder_T sb{};
    nksb_init_alloc(&sb, tmp_alloc);
    inspect(bc_proc.instrs, &sb);
    auto ir_str = nksb_concat(&sb);
    NK_LOG_INF("proc %.*s\n%.*s", (int)ir_proc.name.size, ir_proc.name.data, (int)ir_str.size, ir_str.data);
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

void nkir_invoke(NkIrRunCtx ctx, NkIrProc proc_id, void **args, void **ret) {
    NK_LOG_TRC("%s", __func__);

    translateProc(ctx, proc_id);
    nkir_interp_invoke(ctx->procs[proc_id.id], args, ret);
}
