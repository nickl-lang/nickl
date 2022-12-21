#include "bytecode.h"

#include <cassert>
#include <new>
#include <tuple>

#include "bytecode_impl.hpp"
#include "interp.hpp"
#include "ir_impl.hpp"
#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"
#include "nk/vm/value.h"

char const *s_nk_bc_names[] = {
#define X(NAME) #NAME,
#define XE(NAME, VAR) #NAME " (" #VAR ")",
#include "bytecode.inl"
};

namespace {

NK_LOG_USE_SCOPE(bytecode);

NkOpCode s_ir2opcode[] = {
#define X(NAME) CAT(nkop_, NAME),
#include "nk/vm/ir.inl"
};

void _inspect(NkBcProg p, size_t first_instr, size_t last_instr, NkStringBuilder sb) {
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
        case NkBcRef_Global:
            nksb_printf(sb, "global+");
            break;
        case NkBcRef_Const:
            // TODO NkBcRef_Const??
            break;
        case NkBcRef_Reg:
            nksb_printf(sb, "reg+");
            break;
        case NkBcRef_Instr:
            nksb_printf(sb, "instr+");
            break;
        case NkBcRef_Abs:
            nkval_inspect(nkval_t{(void *)arg.offset, arg.type}, sb);
            break;
        default:
            assert(!"unreachable");
            break;
        }
        if (arg.ref_type != NkBcRef_Const && arg.ref_type != NkBcRef_Abs) {
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

    for (size_t i = first_instr; i <= last_instr; i++) {
        auto const &instr = p->instrs[i];

        nksb_printf(sb, "%zx\t", (&instr - p->instrs.data()) * sizeof(NkBcInstr));

        if (instr.arg[0].ref_type != NkBcRef_None) {
            _inspectArg(instr.arg[0]);
            nksb_printf(sb, " := ");
        }

        nksb_printf(sb, s_nk_bc_names[instr.code]);

        for (size_t i = 1; i < 3; i++) {
            if (instr.arg[i].ref_type != NkBcRef_None) {
                nksb_printf(sb, ((i > 1) ? ", " : " "));
                _inspectArg(instr.arg[i]);
            }
        }

        nksb_printf(sb, "\n");
    }
}

BytecodeFunct _translateIr(NkBcProg p, NkIrFunctId fn) {
    NK_LOG_DBG("translating funct id=%llu", fn.id);

    auto const &ir = *p->ir;

    auto const &ir_funct = ir.functs[fn.id];

    auto frame_layout =
        nk_calcTupleLayout(ir_funct.locals.data(), ir_funct.locals.size(), nk_default_allocator, 1);
    defer {
        nk_free(nk_default_allocator, frame_layout.info_ar.data);
    };

    BytecodeFunct bc_funct{
        .prog = p,
        .frame_size = frame_layout.size,
        .first_instr = p->instrs.size(),
        .fn_t = ir_funct.fn_t,
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

    auto _compileArg = [&](size_t ii, size_t ai, NkBcRef &arg, NkIrArg const &ir_arg) {
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
                arg.offset += ir_funct.fn_t->as.fn.args_t->as.tuple.elems.data[ref.index].offset;
                break;
            case NkIrRef_Ret:
                break;
            case NkIrRef_Global:
                // TODO arg.offset += globals_layout.info_ar[ref.value.index].offset;
                break;
            case NkIrRef_Const:
                arg.ref_type = NkBcRef_Abs;
                arg.offset = (size_t)ref.data;
                break;
            case NkIrRef_Reg:
                arg.offset += ref.index * REG_SIZE;
                break;
            case NkIrRef_ExtVar:
                // TODO arg.ref_type = NkBcRef_Abs;
                // arg.offset = (size_t)prog.exsyms[ir_arg.id];
                break;
            default:
                assert(!"unreachable");
            case NkIrRef_None:
                break;
            }
            break;
        }
        case NkIrArg_BlockId:
            // TODO arg.ref_type = NkBcRef_Instr;
            // *relocs.push() = {
            //     .instr_index = ii,
            //     .arg = ai,
            //     .target_id = ir_arg.id,
            //     .reloc_type = Reloc_Block,
            // };
            // break;
        case NkIrArg_FunctId:
            // TODO arg.ref_type = NkBcRef_Const;
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

    for (auto block_id : ir_funct.blocks) {
        auto const &block = ir.blocks[block_id];

        // TODO???????????? auto &block_info = block_info_ar[block.id] = {};
        // block_info.first_instr = prog.instrs.size;

        for (auto const &ir_instr_id : block.instrs) {
            auto const &ir_instr = ir.instrs[ir_instr_id];

            uint16_t code = s_ir2opcode[ir_instr.code];

            auto const &arg1 = ir_instr.arg[1];

            switch (ir_instr.code) {
            case nkir_call:
                code = nkop_call; // TODO Not doing jump call
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
#include "bytecode.inl"
                code += 1 + NUM_TYPE_INDEX(arg1.ref.type->as.num.value_type);
                break;
            }

            auto &instr = p->instrs.emplace_back();
            instr.code = code;
            for (size_t ai = 0; ai < 3; ai++) {
                _compileArg(p->instrs.size() - 1, ai, instr.arg[ai], ir_instr.arg[ai]);
            }
        }
    }

    {
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };

        _inspect(p, bc_funct.first_instr, p->instrs.size() - 1, sb);
        auto str = nksb_concat(sb);

        NK_LOG_INF("bytecode:\n%.*s", str.size, str.data);
    }

    return bc_funct;
}

} // namespace

NkBcProg nkbc_createProgram(NkIrProg ir) {
    auto prog = new (nk_allocate(nk_default_allocator, sizeof(NkBcProg_T))) NkBcProg_T{
        .ir = ir,
        .arena = nk_create_arena(),
    };
    prog->instrs.reserve(100); // TODO Huge hack that avoids instruction reallocation
    return prog;
}

void nkbc_deinitProgram(NkBcProg p) {
    if (p) {
        nk_free_arena(p->arena);
        p->~NkBcProg_T();
        nk_free(nk_default_allocator, p);
    }
}

void nkbc_invoke(NkBcProg p, NkIrFunctId fn, nkval_t ret, nkval_t args) {
    //@TODO Add trace/debug logs

    assert(fn.id < p->ir->functs.size() && "invalid function");

    auto it = p->functs.find(fn.id);
    if (it == std::end(p->functs)) {
        std::tie(it, std::ignore) = p->functs.emplace(fn.id, _translateIr(p, fn));
    }

    nk_interp_invoke(it->second, ret, args);
}
