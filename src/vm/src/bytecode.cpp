#include "bytecode.h"

#include <cstring>
#include <vector>

#include "bytecode_impl.hpp"
#include "dl_adapter.h"
#include "interp.hpp"
#include "ir_impl.hpp"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"
#include "nk/vm/value.h"
#include "ntk/allocator.h"
#include "ntk/log.h"
#include "ntk/utils.h"

char const *s_nk_bc_names[] = {
#define X(NAME) #NAME,
#define XE(NAME, VAR) #NAME " (" #VAR ")",
#include "bytecode.inl"
};

namespace {

NK_LOG_USE_SCOPE(bytecode);

NkOpCode s_ir2opcode[] = {
#define X(NAME) NK_CAT(nkop_, NAME),
#include "nk/vm/ir.inl"
};

#ifdef ENABLE_LOGGING
void _inspect(std::vector<NkBcInstr> const &instrs, NkStringBuilder *sb) {
    auto _inspectArg = [&](NkBcRef const &arg) {
        if (arg.is_indirect) {
            nksb_printf(sb, "[");
        }
        switch (arg.ref_type) {
            case NkBcRef_Frame:
                nksb_printf(sb, "frame+");
                break;
            case NkBcRef_Arg:
                nksb_printf(sb, "arg+");
                break;
            case NkBcRef_Ret:
                nksb_printf(sb, "ret+");
                break;
            case NkBcRef_Reg:
                nksb_printf(sb, "reg+");
                break;
            case NkBcRef_Rodata:
                nkval_inspect({(void *)arg.offset, arg.type}, sb);
                break;
            case NkBcRef_Data:
                nksb_printf(sb, "data+");
                break;
            case NkBcRef_Instr:
                nksb_printf(sb, "instr+");
                break;
            default:
                nk_assert(!"unreachable");
                break;
        }
        if (arg.ref_type != NkBcRef_Rodata) {
            nksb_printf(sb, "%zx", arg.offset);
        }
        if (arg.is_indirect) {
            nksb_printf(sb, "]");
        }
        if (arg.post_offset) {
            nksb_printf(sb, "+%zx", arg.post_offset);
        }
        if (arg.type) {
            nksb_printf(sb, ":");
            nkt_inspect(arg.type, sb);
        }
    };

    for (auto const &instr : instrs) {
        nksb_printf(sb, "%zx\t", (&instr - instrs.data()) * sizeof(NkBcInstr));

        if (instr.arg[0].ref_type != NkBcRef_None) {
            _inspectArg(instr.arg[0]);
            nksb_printf(sb, " := ");
        }

        nksb_printf(sb, "%s", s_nk_bc_names[instr.code]);

        for (usize i = 1; i < 3; i++) {
            if (instr.arg[i].ref_type != NkBcRef_None) {
                nksb_printf(sb, ((i > 1) ? ", " : " "));
                _inspectArg(instr.arg[i]);
            }
        }

        nksb_printf(sb, "\n");
    }
}
#endif // ENABLE_LOGGING

NkBcFunct _translateIr(NkBcProg p, NkIrFunct fn) {
    NK_LOG_DBG("Translating funct `%s`", fn->name.c_str());

    auto const &ir = *p->ir;

    auto frame_layout = nk_calcTupleLayout(fn->locals.data(), fn->locals.size(), nk_default_allocator, 1);
    defer {
        nk_free(nk_default_allocator, frame_layout.info_ar.data, frame_layout.info_ar.size);
    };

    auto &instrs = p->instrs.emplace_back();

    auto &bc_funct = p->functs.emplace_back(NkBcFunct_T{
        .prog = p,
        .frame_size = frame_layout.size,
        .instrs = nullptr,
        .fn_t = fn->fn_t,
    });

    enum ERelocType {
        Reloc_Block,
    };

    struct Reloc {
        usize instr_index;
        usize arg;
        usize target_id;
        ERelocType reloc_type;
    };

    struct BlockInfo {
        usize first_instr;
    };

    std::vector<BlockInfo> block_info;
    block_info.resize(ir.blocks.size(), {});

    std::vector<Reloc> relocs{};

    auto _compileArg = [&](usize ii, usize ai, NkBcRef &arg, NkIrArg const &ir_arg) {
        switch (ir_arg.arg_type) {
            case NkIrArg_None:
                arg.ref_type = NkBcRef_None;
                break;
            case NkIrArg_Ref: {
                auto const &ref = ir_arg.ref;
                arg.offset += ref.offset;
                arg.post_offset = ref.post_offset;
                arg.type = ref.type;
                arg.ref_type = (NkBcRefType)ref.ref_type;
                arg.is_indirect = ref.is_indirect;
                switch (ref.ref_type) {
                    case NkIrRef_Frame:
                        arg.offset += frame_layout.info_ar.data[ref.index].offset;
                        break;
                    case NkIrRef_Arg:
                        arg.offset += fn->fn_t->as.fn.args_t->as.tuple.elems.data[ref.index].offset;
                        break;
                    case NkIrRef_Ret:
                        break;
                    case NkIrRef_Global: {
                        arg.ref_type = NkBcRef_Data;
                        if (ref.index >= p->globals.size()) {
                            p->globals.resize(ref.index + 1, {});
                        }
                        auto &val = p->globals[ref.index];
                        if (!val.data) {
                            auto const type = ir.globals[ref.index];
                            val = {nk_arena_alloc(&p->arena, type->size), type};
                            std::memset(val.data, 0, type->size);
                        }
                        arg.offset += (usize)val.data;
                        break;
                    }
                    case NkIrRef_Const: {
                        arg.ref_type = NkBcRef_Rodata;
                        auto const_data = nkval_data(ir.consts[ref.index]);
                        arg.offset = (usize)const_data;
                        break;
                    }
                    case NkIrRef_Reg:
                        arg.offset += ref.index * REG_SIZE;
                        break;
                    case NkIrRef_ExtSym: {
                        arg.ref_type = NkBcRef_Rodata;
                        auto const &exsym = ir.exsyms[ref.index];
                        if (exsym.so_id.id >= p->shobjs.size()) {
                            p->shobjs.resize(exsym.so_id.id + 1, {});
                        }
                        auto &dl = p->shobjs[exsym.so_id.id];
                        dl = nkdl_open(nk_cs2s(ir.shobjs[exsym.so_id.id].c_str()));
                        if (ref.index >= p->exsyms.size()) {
                            p->exsyms.resize(ref.index + 1, {});
                        }
                        auto &sym = p->exsyms[ref.index];
                        sym = nkdl_sym(dl, nk_cs2s(exsym.name.c_str()));
                        arg.offset += (usize)&sym;
                        break;
                    }
                    default:
                        nk_assert(!"unreachable");
                    case NkIrRef_None:
                        break;
                }
                break;
            }
            case NkIrArg_BlockId:
                arg.ref_type = NkBcRef_Instr;
                relocs.emplace_back(Reloc{
                    .instr_index = ii,
                    .arg = ai,
                    .target_id = ir_arg.id,
                    .reloc_type = Reloc_Block,
                });
                break;
            case NkIrArg_NumValType: // TODO
                break;
        }
    };

    std::vector<NkIrFunct> referenced_functs;

    for (auto block_id : fn->blocks) {
        auto const &block = ir.blocks[block_id];

        block_info[block_id].first_instr = instrs.size();

        for (auto const &ir_instr_id : block.instrs) {
            auto const &ir_instr = ir.instrs[ir_instr_id];

            u16 code = s_ir2opcode[ir_instr.code];

            auto const &arg1 = ir_instr.arg[1];

            switch (ir_instr.code) {
                case nkir_call:
                    if (arg1.ref.ref_type == NkIrRef_Const && arg1.ref.type->tclass == NkType_Fn &&
                        arg1.ref.type->as.fn.call_conv == NkCallConv_Nk) {
                        auto const fn = nkval_as(NkIrFunct, nkir_refDeref(p->ir, arg1.ref));
                        bool found = false;
                        // TODO Manual search for fn
                        for (auto f : ir.functs) {
                            if (fn == f) {
                                found = true;
                                break;
                            }
                        }
                        if (found) {
                            code = nkop_call_jmp;
                            referenced_functs.emplace_back(fn);
                        }
                    }
                    break;
                case nkir_mov:
                case nkir_jmpz:
                case nkir_jmpnz:
                case nkir_eq:
                case nkir_ne:
                    if (arg1.ref.type->size <= REG_SIZE && nk_isZeroOrPowerOf2(arg1.ref.type->size)) {
                        code += 1 + nk_log2u64(arg1.ref.type->size);
                    }
                    break;
#define NUM(NAME) case NK_CAT(nkir_, NAME):
#define INT(NAME) case NK_CAT(nkir_, NAME):
#include "bytecode.inl"
                    if (arg1.ref.type->tclass == NkType_Ptr) {
                        code += 1 + NUM_TYPE_INDEX(Uint64);
                    } else {
                        nk_assert(arg1.ref.type->tclass == NkType_Numeric);
                        code += 1 + NUM_TYPE_INDEX(arg1.ref.type->as.num.value_type);
                    }
                    break;

                case nkir_cast:
                    nk_assert(arg1.arg_type == NkIrArg_NumValType);
                    nk_assert(ir_instr.arg[2].ref.type->tclass == NkType_Numeric);
                    code += 1 + NUM_TYPE_INDEX(arg1.id) * NUM_TYPE_COUNT +
                            NUM_TYPE_INDEX(ir_instr.arg[2].ref.type->as.num.value_type);
                    break;
            }

            auto &instr = instrs.emplace_back();
            instr.code = code;
            for (usize ai = 0; ai < 3; ai++) {
                _compileArg(instrs.size() - 1, ai, instr.arg[ai], ir_instr.arg[ai]);
            }
        }
    }

    for (auto const &reloc : relocs) {
        NkBcRef &arg = instrs[reloc.instr_index].arg[reloc.arg];

        switch (reloc.reloc_type) {
            case Reloc_Block:
                arg.offset = block_info[reloc.target_id].first_instr * sizeof(NkBcInstr);
                break;
        }
    }

    bc_funct.instrs = instrs.data();

    NK_LOG_INF(
        "bytecode:\n%s", (char const *)[&]() {
            NkStringBuilder sb{};
            _inspect(instrs, &sb);
            nksb_appendNull(&sb);
            return nk_defer((char const *)sb.data, [sb]() mutable {
                nksb_free(&sb);
            });
        }());

    for (auto fn : referenced_functs) {
        if (!fn->bc_funct) {
            fn->bc_funct = _translateIr(p, fn);
        }
    }

    return &bc_funct;
}

} // namespace

NkBcProg nkbc_createProgram(NkIrProg ir) {
    auto prog = new (nk_alloc(nk_default_allocator, sizeof(NkBcProg_T))) NkBcProg_T{
        .ir = ir,
    };
    return prog;
}

void nkbc_deinitProgram(NkBcProg p) {
    if (p) {
        for (auto dl : p->shobjs) {
            nkdl_close(dl);
        }

        nk_arena_free(&p->arena);

        p->~NkBcProg_T();
        nk_free(nk_default_allocator, p, sizeof(*p));
    }
}

void nkbc_invoke(NkBcProg p, NkIrFunct fn, nkval_t ret, nkval_t args) {
    //@TODO Add trace/debug logs

    if (!fn->bc_funct) {
        fn->bc_funct = _translateIr(p, fn);
    }

    nk_interp_invoke(fn->bc_funct, ret, args);
}
