#include "nk/vm/ir.h"

#include <cassert>
#include <new>

#include "ir_impl.hpp"
#include "nk/common/allocator.h"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/string.hpp"
#include "nk/common/string_builder.h"
#include "nk/vm/value.h"

char const *s_nk_ir_names[] = {
#define X(NAME) #NAME,
#include "nk/vm/ir.inl"
};

namespace {

NkIrArg _arg(NkIrRef ref) {
    return {{.ref = ref}, NkIrArg_Ref};
}

NkIrArg _arg(NkIrBlockId block) {
    return {{.id = block.id}, NkIrArg_BlockId};
}

NkIrArg _arg(NkNumericValueType value_type) {
    return {{.id = value_type}, NkIrArg_NumValType};
}

} // namespace

NkIrProg nkir_createProgram() {
    return new (nk_allocate(nk_default_allocator, sizeof(NkIrProg_T))) NkIrProg_T{};
}

void nkir_deinitProgram(NkIrProg p) {
    nkbc_deinitProgram(p->bc);
    for (auto funct : p->functs) {
        funct->~NkIrFunct_T();
        nk_free(nk_default_allocator, funct);
    }
    p->~NkIrProg_T();
    nk_free(nk_default_allocator, p);
}

NkIrFunct nkir_makeFunct(NkIrProg p) {
    return p->functs.emplace_back(new (nk_allocate(nk_default_allocator, sizeof(NkIrFunct_T)))
                                      NkIrFunct_T{
                                          .prog = p,
                                          .fn_t{},
                                      });
}

NkIrBlockId nkir_makeBlock(NkIrProg p) {
    NkIrBlockId id{p->blocks.size()};
    p->blocks.emplace_back(IrBlock{});
    return id;
}

NkIrShObjId nkir_makeShObj(NkIrProg p, nkstr name) {
    NkIrShObjId shobj_id{p->shobjs.size()};
    p->shobjs.emplace_back(std_str(name));
    return shobj_id;
}

void nkir_startFunct(NkIrFunct funct, nkstr name, nktype_t fn_t) {
    funct->name = std_str(name);

    funct->fn_t = fn_t;
    funct->state = NkIrFunct_Complete;

    nkir_activateFunct(funct->prog, funct);
}

void nkir_startIncompleteFunct(NkIrFunct funct, nkstr name, NktFnInfo *fn_info) {
    funct->name = std_str(name);

    funct->fn_info = *fn_info;
    funct->state = NkIrFunct_Incomplete;

    nkir_activateFunct(funct->prog, funct);
}

void nkir_finalizeIncompleteFunct(NkIrFunct funct, NkAllocator *alloc) {
    funct->fn_t = nkt_get_fn(alloc, &funct->fn_info);
    funct->state = NkIrFunct_Complete;
}

void nkir_discardFunct(NkIrFunct funct) {
    auto p = funct->prog;
    auto it = std::find_if(p->functs.begin(), p->functs.end(), [funct](NkIrFunct elem) {
        return elem == funct;
    });
    if (it != p->functs.end()) {
        p->functs.erase(it);
    }
    funct->~NkIrFunct_T();
    nk_free(nk_default_allocator, funct);
}

nktype_t nkir_functGetType(NkIrFunct funct) {
    assert(funct->state == NkIrFunct_Complete && "invalid function state");
    return funct->fn_t;
}

NktFnInfo *nkir_incompleteFunctGetInfo(NkIrFunct funct) {
    assert(funct->state == NkIrFunct_Incomplete && "invalid function state");
    return &funct->fn_info;
}

void nkir_startBlock(NkIrProg p, NkIrBlockId block_id, nkstr name) {
    assert(p->cur_funct && "no current function");
    assert(block_id.id < p->blocks.size() && "invalid block");

    auto &block = p->blocks[block_id.id];
    block.name = std_str(name);

    p->cur_funct->blocks.emplace_back(block_id.id);

    nkir_activateBlock(p, block_id);
}

void nkir_activateFunct(NkIrProg p, NkIrFunct funct) {
    p->cur_funct = funct;
}

void nkir_activateBlock(NkIrProg p, NkIrBlockId block_id) {
    assert(p->cur_funct && "no current function");
    assert(block_id.id < p->blocks.size() && "invalid block");
    p->cur_funct->cur_block = block_id.id;
}

NkIrLocalVarId nkir_makeLocalVar(NkIrProg p, nktype_t type) {
    assert(p->cur_funct && "no current function");
    NkIrLocalVarId id{p->cur_funct->locals.size()};
    p->cur_funct->locals.emplace_back(type);
    return id;
}

NkIrGlobalVarId nkir_makeGlobalVar(NkIrProg p, nktype_t type) {
    NkIrGlobalVarId id{p->globals.size()};
    p->globals.emplace_back(type);
    return id;
}

NkIrExtSymId nkir_makeExtSym(NkIrProg p, NkIrShObjId so, nkstr name, nktype_t type) {
    NkIrExtSymId id{p->exsyms.size()};
    p->exsyms.emplace_back(IrExSym{
        .name = std_str(name),
        .so_id = so,
        .type = type,
    });
    return id;
}

NkIrRef nkir_makeFrameRef(NkIrProg p, NkIrLocalVarId var) {
    assert(p->cur_funct && "no current function");
    return {
        .index = var.id,
        .offset = 0,
        .post_offset = 0,
        .type = p->cur_funct->locals[var.id],
        .ref_type = NkIrRef_Frame,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeArgRef(NkIrProg p, size_t index) {
    assert(p->cur_funct && "no current function");
    assert(
        (p->cur_funct->state == NkIrFunct_Complete || p->cur_funct->fn_info.args_t) &&
        "referencing incomplete function args type");
    auto const args_t = p->cur_funct->state == NkIrFunct_Complete ? p->cur_funct->fn_t->as.fn.args_t
                                                                  : p->cur_funct->fn_info.args_t;
    assert(index < args_t->as.tuple.elems.size && "arg index out of range");
    return {
        .index = index,
        .offset = 0,
        .post_offset = 0,
        .type = args_t->as.tuple.elems.data[index].type,
        .ref_type = NkIrRef_Arg,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeRetRef(NkIrProg p) {
    assert(p->cur_funct && "no current function");
    assert(
        (p->cur_funct->state == NkIrFunct_Complete || p->cur_funct->fn_info.ret_t) &&
        "referencing incomplete function ret type");
    auto const ret_t = p->cur_funct->state == NkIrFunct_Complete ? p->cur_funct->fn_t->as.fn.ret_t
                                                                 : p->cur_funct->fn_info.ret_t;
    return {
        .data = {},
        .offset = 0,
        .post_offset = 0,
        .type = ret_t,
        .ref_type = NkIrRef_Ret,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeGlobalRef(NkIrProg p, NkIrGlobalVarId var) {
    return {
        .index = var.id,
        .offset = 0,
        .post_offset = 0,
        .type = p->globals[var.id],
        .ref_type = NkIrRef_Global,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeConstRef(NkIrProg, nkval_t val) {
    return {
        .data = nkval_data(val),
        .offset = 0,
        .post_offset = 0,
        .type = nkval_typeof(val),
        .ref_type = NkIrRef_Const,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeRegRef(NkIrProg, NkIrRegister reg, nktype_t type) {
    assert(type->size <= REG_SIZE && "reference type excedes register size");
    return {
        .index = reg,
        .offset = 0,
        .post_offset = 0,
        .type = type,
        .ref_type = NkIrRef_Reg,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeExtSymRef(NkIrProg p, NkIrExtSymId sym) {
    return {
        .index = sym.id,
        .offset = 0,
        .post_offset = 0,
        .type = p->exsyms[sym.id].type,
        .ref_type = NkIrRef_ExtSym,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeFunctRef(NkIrFunct funct) {
    assert(funct->state == NkIrFunct_Complete && "referencing incomplete function");
    return {
        .data = funct,
        .offset = 0,
        .post_offset = 0,
        .type = funct->fn_t,
        .ref_type = NkIrRef_Funct,
        .is_indirect = false,
    };
}

NkIrInstr nkir_make_nop() {
    return {{}, nkir_nop};
}

NkIrInstr nkir_make_enter() {
    return {{}, nkir_enter};
}

NkIrInstr nkir_make_leave() {
    return {{}, nkir_leave};
}

NkIrInstr nkir_make_ret() {
    return {{}, nkir_ret};
}

NkIrInstr nkir_make_jmp(NkIrBlockId label) {
    return {{{}, _arg(label)}, nkir_jmp};
}

NkIrInstr nkir_make_jmpz(NkIrRef cond, NkIrBlockId label) {
    return {{{}, _arg(cond), _arg(label)}, nkir_jmpz};
}

NkIrInstr nkir_make_jmpnz(NkIrRef cond, NkIrBlockId label) {
    return {{{}, _arg(cond), _arg(label)}, nkir_jmpnz};
}

NkIrInstr nkir_make_cast(NkIrRef dst, nktype_t type, NkIrRef arg) {
    assert(type->typeclass_id == NkType_Numeric && "numeric type expected in cast");
    return {{_arg(dst), _arg(type->as.num.value_type), _arg(arg)}, nkir_cast};
}

NkIrInstr nkir_make_call(NkIrRef dst, NkIrRef fn, NkIrRef args) {
    return {{_arg(dst), _arg(fn), _arg(args)}, nkir_call};
}

#define U(NAME)                                            \
    NkIrInstr nkir_make_##NAME(NkIrRef dst, NkIrRef arg) { \
        return {{_arg(dst), _arg(arg), {}}, nkir_##NAME};  \
    }
#define B(NAME)                                                         \
    NkIrInstr nkir_make_##NAME(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) { \
        return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_##NAME};        \
    }
#include "nk/vm/ir.inl"

void nkir_gen(NkIrProg p, NkIrInstr instr) {
    assert(p->cur_funct && "no current function");
    assert(p->cur_funct->cur_block < p->blocks.size() && "no current block");
    assert(
        instr.arg[0].arg_type != NkIrArg_Ref || instr.arg[0].ref.is_indirect ||
        (instr.arg[0].ref.ref_type != NkIrRef_Const && instr.arg[0].ref.ref_type != NkIrRef_Arg));

    auto &instrs = p->instrs;
    auto &block = p->blocks[p->cur_funct->cur_block].instrs;

    if (instr.code == nkir_ret && block.size() && instrs[block.back()].code == nkir_ret) {
        return;
    }

    size_t id = instrs.size();
    instrs.emplace_back(instr);
    block.emplace_back(id);
}

void nkir_inspect(NkIrProg p, NkStringBuilder sb) {
    for (auto funct : p->functs) {
        nkir_inspectFunct(funct, sb);
    }

    nkir_inspectExtSyms(p, sb);
}

void nkir_inspectRef(NkIrProg p, NkIrRef ref, NkStringBuilder sb) {
    if (ref.ref_type == NkIrRef_None) {
        nksb_printf(sb, "{}");
        return;
    }
    if (ref.is_indirect) {
        nksb_printf(sb, "[");
    }
    switch (ref.ref_type) {
    case NkIrRef_Frame:
        nksb_printf(sb, "$%llu", ref.index);
        break;
    case NkIrRef_Arg:
        nksb_printf(sb, "$arg%llu", ref.index);
        break;
    case NkIrRef_Ret:
        nksb_printf(sb, "$ret");
        break;
    case NkIrRef_Global:
        nksb_printf(sb, "$global%llu", ref.index);
        break;
    case NkIrRef_Const:
        nkval_inspect({ref.data, ref.type}, sb);
        break;
    case NkIrRef_Reg:
        nksb_printf(sb, "$r%c", (char)('a' + ref.index));
        break;
    case NkIrRef_ExtSym:
        nksb_printf(sb, "(%s)", p->exsyms[ref.index].name.c_str());
        break;
    case NkIrRef_Funct:
        nksb_printf(sb, "%s", ((NkIrFunct)ref.data)->name.c_str());
        break;
    default:
        break;
    }
    if (ref.offset) {
        nksb_printf(sb, "+%llu", ref.offset);
    }
    if (ref.is_indirect) {
        nksb_printf(sb, "]");
    }
    if (ref.post_offset) {
        nksb_printf(sb, "+%llu", ref.post_offset);
    }
    nksb_printf(sb, ":");
    nkt_inspect(ref.type, sb);
}

void nkir_inspectFunct(NkIrFunct funct, NkStringBuilder sb) {
    auto p = funct->prog;

    nksb_printf(sb, "\nfn ");

    assert(funct->state == NkIrFunct_Complete && "inspecting incomplete function");

    switch (funct->fn_t->as.fn.call_conv) {
    case NkCallConv_Nk:
        break;
    case NkCallConv_Cdecl:
        nksb_printf(sb, "(cdecl) ");
        break;
    }

    nksb_printf(sb, "%s(", funct->name.c_str());
    for (size_t i = 0; i < funct->fn_t->as.fn.args_t->as.tuple.elems.size; i++) {
        if (i) {
            nksb_printf(sb, ", ");
        }
        nksb_printf(sb, "$arg%llu:", i);
        nkt_inspect(funct->fn_t->as.fn.args_t->as.tuple.elems.data[i].type, sb);
    }

    nksb_printf(sb, ") -> ");
    nkt_inspect(funct->fn_t->as.fn.ret_t, sb);

    if (!funct->locals.empty()) {
        nksb_printf(sb, "\n\n");
        for (size_t i = 0; i < funct->locals.size(); i++) {
            nksb_printf(sb, "$%llu: ", i);
            nkt_inspect(funct->locals[i], sb);
            nksb_printf(sb, "\n");
        }
        nksb_printf(sb, "\n");
    } else {
        nksb_printf(sb, " ");
    }

    nksb_printf(sb, "{\n\n");

    for (auto block_id : funct->blocks) {
        auto const &block = p->blocks[block_id];

        nksb_printf(sb, "%%%s:\n", block.name.c_str());

        for (auto instr_id : block.instrs) {
            auto const &instr = p->instrs[instr_id];

            nksb_printf(sb, "  ");

            if (instr.arg[0].arg_type == NkIrArg_Ref && instr.arg[0].ref.ref_type != NkIrRef_None) {
                nkir_inspectRef(p, instr.arg[0].ref, sb);
                nksb_printf(sb, " := ");
            }

            nksb_printf(sb, s_nk_ir_names[instr.code]);

            for (size_t i = 1; i < 3; i++) {
                auto &arg = instr.arg[i];
                if (arg.arg_type != NkIrArg_None) {
                    nksb_printf(sb, ((i > 1) ? ", " : " "));
                }
                switch (arg.arg_type) {
                case NkIrArg_Ref: {
                    auto &ref = arg.ref;
                    nkir_inspectRef(p, ref, sb);
                    break;
                }
                case NkIrArg_BlockId:
                    if (arg.id < p->blocks.size() && !p->blocks[arg.id].name.empty()) {
                        nksb_printf(sb, "%%%s", p->blocks[arg.id].name.c_str());
                    } else {
                        nksb_printf(sb, "%(null)");
                    }
                    break;
                case NkIrArg_NumValType:
                    switch (arg.id) {
                    case Int8:
                    case Int16:
                    case Int32:
                    case Int64:
                        nksb_printf(sb, "i");
                        break;
                    case Uint8:
                    case Uint16:
                    case Uint32:
                    case Uint64:
                        nksb_printf(sb, "u");
                        break;
                    case Float32:
                    case Float64:
                        nksb_printf(sb, "f");
                        break;
                    default:
                        assert(!"unreachable");
                        break;
                    }
                    nksb_printf(sb, "%llu", (size_t)NUM_TYPE_SIZE(arg.id) * 8);
                    break;
                case NkIrArg_None:
                default:
                    break;
                }
            }

            nksb_printf(sb, "\n");
        }

        nksb_printf(sb, "\n");
    }

    nksb_printf(sb, "}\n");
}

void nkir_inspectExtSyms(NkIrProg p, NkStringBuilder sb) {
    if (p->exsyms.size()) {
        for (auto const &sym : p->exsyms) {
            nksb_printf(sb, "\n\"%s\" %s:", p->shobjs[sym.so_id.id].c_str(), sym.name.c_str());
            nkt_inspect(sym.type, sb);
        }
        nksb_printf(sb, "\n");
    }
}

void nkir_invoke(nkval_t fn_val, nkval_t ret, nkval_t args) {
    auto fn = nkval_as(NkIrFunct, fn_val);

    if (!fn->prog->bc) {
        fn->prog->bc = nkbc_createProgram(fn->prog);
    }

    nkbc_invoke(fn->prog->bc, fn, ret, args);
}
