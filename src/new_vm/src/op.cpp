#include "op.h"

#include <cassert>
#include <new>
#include <tuple>

#include "interp.hpp"
#include "ir_internal.hpp"
#include "nk/common/allocator.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"
#include "nk/vm/value.h"
#include "op_internal.hpp"

namespace {

NkOpCode s_ir2opcode[] = {
#define X(NAME) CAT(nkop_, NAME),
#include "nk/vm/ir.inl"
};

FunctInfo _translateIr(NkOpProg prog, NkIrFunctId fn) {
    auto const &ir = *prog->ir;

    auto const &funct = ir.functs[fn.id];

    auto frame_layout =
        nk_calcTupleLayout(funct.locals.data(), funct.locals.size(), nk_default_allocator, 1);
    defer {
        nk_free(nk_default_allocator, frame_layout.info_ar.data);
    };

    FunctInfo funct_info{
        .prog = prog,
        .frame_size = frame_layout.size,
        .frame_align = frame_layout.align,
        .first_instr = prog->instrs.size(),
        .instr_count = 0,
        .fn_t = funct.fn_t,
    };

    // TODO enum ERelocType {
    //     Reloc_Funct,
    //     Reloc_Block,
    // };

    // struct Reloc {
    //     size_t instr_index;
    //     size_t arg;
    //     size_t target_id;
    //     ERelocType reloc_type;
    // };

    // std::vector<Reloc> relocs{};

    auto _compileArg = [&](size_t ii, size_t ai, NkOpRef &arg, NkIrArg const &ir_arg) {
        switch (ir_arg.arg_type) {
        case NkIrArg_None:
            arg.ref_type = NkOpRef_None;
            break;
        case NkIrArg_Ref: {
            auto const &ref = ir_arg.ref;
            arg.offset += ref.offset;
            arg.post_offset = ref.post_offset;
            arg.type = ref.type;
            arg.ref_type = (NkOpRefType)ref.ref_type;
            arg.is_indirect = ref.is_indirect;
            switch (ref.ref_type) {
            case NkIrRef_Frame:
                arg.offset += frame_layout.info_ar.data[ref.index].offset;
                break;
            case NkIrRef_Arg:
                arg.offset += funct.fn_t->as.fn.args_t->as.tuple.elems.data[ref.index].offset;
                break;
            case NkIrRef_Ret:
                break;
            case NkIrRef_Global:
                // TODO arg.offset += globals_layout.info_ar[ref.value.index].offset;
                break;
            case NkIrRef_Const:
                // TODO arg.offset += _pushConst({ref.value.data, ref.type});
                break;
            case NkIrRef_Reg:
                arg.offset += ref.index * REG_SIZE;
                break;
            case NkIrRef_ExtVar: {
                // TODO arg.ref_type = NkOpRef_Abs;
                // arg.offset = (size_t)prog.exsyms[ir_arg.id];
                break;
            }
            default:
                assert(!"unreachable");
            case NkIrRef_None:
                break;
            }
            break;
        }
        case NkIrArg_BlockId:
            // TODO arg.ref_type = NkOpRef_Instr;
            // *relocs.push() = {
            //     .instr_index = ii,
            //     .arg = ai,
            //     .target_id = ir_arg.id,
            //     .reloc_type = Reloc_Block,
            // };
            // break;
        case NkIrArg_FunctId:
            // TODO arg.ref_type = NkOpRef_Const;
            // *relocs.push() = {
            //     .instr_index = ii,
            //     .arg = ai,
            //     .target_id = ir_arg.id,
            //     .reloc_type = Reloc_Funct,
            // };
            // break;
        case NkIrArg_ExtFunctId: {
            // TODO auto &exsym = ir.exsyms[ir_arg.id];
            // arg.ref_type = Ref_Const;
            // arg.type = types::get_fn_native(
            //     exsym.as.funct.ret_t,
            //     exsym.as.funct.args_t,
            //     0,
            //     prog.exsyms[ir_arg.id],
            //     exsym.as.funct.is_variadic);
            // break;
        }
        case NkIrArg_NumValType: {
            // TODO
            break;
        }
        }
    };

    for (auto block_id : funct.blocks) {
        auto const &block = ir.blocks[block_id];

        // TODO???????????? auto &block_info = block_info_ar[block.id] = {};
        // block_info.first_instr = prog.instrs.size;

        for (auto const &ir_instr_id : block.instrs) {
            auto const &ir_instr = ir.instrs[ir_instr_id];

            uint16_t code = s_ir2opcode[ir_instr.code];

            auto const &arg1 = ir_instr.arg[1];

            switch (ir_instr.code) {
            case nkir_call:
                // TODO if (arg1.arg_type == NkIrArg_FunctId) {
                //     code = op_call_jmp;
                //     if (!prog.funct_info[arg1.id].prog) {
                //         funct_ids_to_translate.insert(arg1.id);
                //     }
                // }
                break;
            case nkir_mov:
            case nkir_eq:
            case nkir_ne:
                if (arg1.ref.type->size <= REG_SIZE && isZeroOrPowerOf2(arg1.ref.type->size)) {
                    code += 1 + log2u(arg1.ref.type->size);
                }
                break;
#define NUM_X(NAME) case CAT(nkir_, NAME):
#define NUM_INT_X(NAME) case CAT(nkir_, NAME):
#include "op.inl"
                code += 1 + NUM_TYPE_INDEX(arg1.ref.type->as.num.value_type);
                break;
            }

            auto &instr = prog->instrs.emplace_back();
            funct_info.instr_count++;
            instr.code = code;
            for (size_t ai = 0; ai < 3; ai++) {
                _compileArg(prog->instrs.size() - 1, ai, instr.arg[ai], ir_instr.arg[ai]);
            }
        }
    }

    return funct_info;
}

} // namespace

NkOpProg nkop_createProgram(NkIrProg ir) {
    return new (nk_allocate(nk_default_allocator, sizeof(NkOpProg_T))) NkOpProg_T{
        .ir = ir,
        .arena = nk_create_arena(),
    };
}

void nkop_deinitProgram(NkOpProg p) {
    if (p) {
        nk_free_arena(p->arena);
        p->~NkOpProg_T();
        nk_free(nk_default_allocator, p);
    }
}

void nkop_invoke(NkOpProg p, NkIrFunctId fn, nkval_t ret, nkval_t args) {
    //@TODO Add trace/debug logs

    assert(fn.id < p->ir->functs.size() && "invalid function");

    auto it = p->functs.find(fn.id);
    if (it == std::end(p->functs)) {
        std::tie(it, std::ignore) = p->functs.emplace(fn.id, _translateIr(p, fn));
    }

    nk_interp_invoke(it->second, ret, args);
}
