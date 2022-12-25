#include "bytecode.h"

#include <cassert>
#include <new>
#include <tuple>
#include <vector>

#include "bytecode_impl.hpp"
#include "dl_adapter.h"
#include "interp.hpp"
#include "ir_impl.hpp"
#include "native_fn_adapter.h"
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

void _inspect(std::vector<NkBcInstr> const &instrs, NkStringBuilder sb) {
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
        case NkBcRef_Instr:
            nksb_printf(sb, "instr+");
            break;
        case NkBcRef_Abs:
            nkval_inspect({(void *)arg.offset, arg.type}, sb);
            break;
        default:
            assert(!"unreachable");
            break;
        }
        if (arg.ref_type != NkBcRef_Abs) {
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

NkBcFunct _translateIr(NkBcProg p, NkIrFunct fn) {
    NK_LOG_DBG("Translating funct `%s`", fn->name.c_str());

    auto const &ir = *p->ir;

    auto frame_layout =
        nk_calcTupleLayout(fn->locals.data(), fn->locals.size(), nk_default_allocator, 1);
    defer {
        nk_free(nk_default_allocator, frame_layout.info_ar.data);
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
        size_t instr_index;
        size_t arg;
        size_t target_id;
        ERelocType reloc_type;
    };

    struct BlockInfo {
        size_t first_instr;
    };

    std::vector<BlockInfo> block_info;
    block_info.resize(ir.blocks.size());

    std::vector<Reloc> relocs{};

    std::vector<NkIrFunct> referenced_functs;

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
                arg.offset += fn->fn_t->as.fn.args_t->as.tuple.elems.data[ref.index].offset;
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
            case NkIrRef_ExtSym: {
                arg.ref_type = NkBcRef_Abs;
                auto const &exsym = ir.exsyms[ref.index];
                if (exsym.so_id.id >= p->shobjs.size()) {
                    p->shobjs.resize(exsym.so_id.id + 1);
                }
                auto &dl = p->shobjs[exsym.so_id.id];
                dl = nkdl_open(cs2s(ir.shobjs[exsym.so_id.id].c_str()));
                if (ref.index >= p->exsyms.size()) {
                    p->exsyms.resize(ref.index + 1);
                }
                auto &sym = p->exsyms[ref.index];
                sym = nkdl_sym(dl, cs2s(exsym.name.c_str()));
                arg.offset = (size_t)&sym;
                break;
            }
            case NkIrRef_Funct:
                referenced_functs.emplace_back((NkIrFunct)ref.data);
                arg.ref_type = NkBcRef_Abs;
                switch (ref.type->as.fn.call_conv) {
                case NkCallConv_Nk:
                    arg.offset = (size_t)&ref.data;
                    break;
                case NkCallConv_Cdecl:
                    arg.offset = (size_t)p->closures.emplace_back(
                        nk_native_make_closure({(void *)&ref.data, ref.type}));
                    break;
                default:
                    assert(!"unreachable");
                }
                break;
            default:
                assert(!"unreachable");
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

    for (auto block_id : fn->blocks) {
        auto const &block = ir.blocks[block_id];

        block_info[block_id].first_instr = instrs.size();

        for (auto const &ir_instr_id : block.instrs) {
            auto const &ir_instr = ir.instrs[ir_instr_id];

            uint16_t code = s_ir2opcode[ir_instr.code];

            auto const &arg1 = ir_instr.arg[1];

            switch (ir_instr.code) {
            case nkir_call:
                if (arg1.ref.ref_type == NkIrRef_Funct) {
                    code = nkop_call_jmp;
                }
                break;
            case nkir_mov:
            case nkir_jmpz:
            case nkir_jmpnz:
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

            auto &instr = instrs.emplace_back();
            instr.code = code;
            for (size_t ai = 0; ai < 3; ai++) {
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

    {
        // TODO Inspecting bytecode in _translateIr outside of the log macro
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };

        _inspect(instrs, sb);
        auto str = nksb_concat(sb);

        NK_LOG_INF("bytecode:\n%.*s", str.size, str.data);
    }

    for (auto fn : referenced_functs) {
        if (!fn->bc_funct) {
            fn->bc_funct = _translateIr(p, fn);
        }
    }

    return &bc_funct;
}

} // namespace

NkBcProg nkbc_createProgram(NkIrProg ir) {
    auto prog = new (nk_allocate(nk_default_allocator, sizeof(NkBcProg_T))) NkBcProg_T{
        .ir = ir,
    };
    return prog;
}

void nkbc_deinitProgram(NkBcProg p) {
    if (p) {
        for (auto dl : p->shobjs) {
            nkdl_close(dl);
        }
        for (auto cl : p->closures) {
            nk_native_free_closure(cl);
        }

        p->~NkBcProg_T();
        nk_free(nk_default_allocator, p);
    }
}

void nkbc_invoke(NkBcProg p, NkIrFunct fn, nkval_t ret, nkval_t args) {
    //@TODO Add trace/debug logs

    if (!fn->bc_funct) {
        fn->bc_funct = _translateIr(p, fn);
    }

    nk_interp_invoke(fn->bc_funct, ret, args);
}
