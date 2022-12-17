#include "nk/vm/ir.h"

#include <cassert>
#include <deque>
#include <new>
#include <string>
#include <vector>

#include "nk/common/allocator.h"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/string.hpp"
#include "nk/common/string_builder.h"
#include "nk/vm/common.h"

char const *s_nk_ir_names[] = {
#define X(NAME) #NAME,
#include "nk/vm/ir.inl"
};

namespace {

DEFINE_ID_TYPE(NkIrInstrId);

struct IrFunct {
    std::string name{};
    nk_type_t fn_t{};

    std::vector<NkIrBlockId> blocks{};
    std::vector<nk_type_t> locals{};
};

struct IrBlock {
    std::string name{};

    std::vector<NkIrInstrId> instrs{};
};

struct IrExSym {
    std::string name;
    NkIrShObjId so_id;
    nk_type_t type;
};

NkIrArg _arg(NkIrRefPtr ref) {
    return {{.ref = *ref}, NkIrArg_Ref};
}

NkIrArg _arg(NkIrBlockId block) {
    return {{.id = block.id}, NkIrArg_BlockId};
}

NkIrArg _arg(NkIrFunctId funct) {
    return {{.id = funct.id}, NkIrArg_FunctId};
}

NkIrArg _arg(NkIrExtFunctId funct) {
    return {{.id = funct.id}, NkIrArg_ExtFunctId};
}

} // namespace

struct NkIrProg_T {
    IrFunct *cur_funct{};
    IrBlock *cur_block{};

    std::deque<IrFunct> functs{};
    std::deque<IrBlock> blocks{};
    std::vector<NkIrInstr> instrs{};
    std::vector<std::string> shobjs{};
    std::vector<nk_type_t> globals{};
    std::vector<IrExSym> exsyms{};
};

NkIrProg nkir_createProgram() {
    return new (nk_allocate(nk_default_allocator, sizeof(NkIrProg_T))) NkIrProg_T{};
}

void nkir_deinitProgram(NkIrProg p) {
    p->~NkIrProg_T();
    nk_free(nk_default_allocator, p);
}

NkIrFunctId nkir_makeFunct(NkIrProg p) {
    NkIrFunctId id{p->functs.size()};
    p->functs.emplace_back();
    return id;
}

NkIrBlockId nkir_makeBlock(NkIrProg p) {
    NkIrBlockId id{p->blocks.size()};
    p->blocks.emplace_back();
    return id;
}

NkIrShObjId nkir_makeShObj(NkIrProg p, nkstr name) {
    NkIrShObjId shobj_id{p->shobjs.size()};
    p->shobjs.emplace_back(std_str(name));
    return shobj_id;
}

void nkir_startFunct(NkIrProg p, NkIrFunctId funct_id, nkstr name, nk_type_t fn_t) {
    assert(funct_id.id < p->functs.size() && "invalid function");

    auto &funct = p->functs[funct_id.id];
    funct.name = std_str(name);
    funct.fn_t = fn_t;

    nkir_activateFunct(p, funct_id);
}

void nkir_startBlock(NkIrProg p, NkIrBlockId block_id, nkstr name) {
    assert(p->cur_funct && "no current function");
    assert(block_id.id < p->blocks.size() && "invalid block");

    auto &block = p->blocks[block_id.id];
    block.name = std_str(name);

    nkir_activateBlock(p, block_id);
}

void nkir_activateFunct(NkIrProg p, NkIrFunctId funct_id) {
    assert(funct_id.id < p->functs.size() && "invalid function");
    p->cur_funct = &p->functs[funct_id.id];
}

void nkir_activateBlock(NkIrProg p, NkIrBlockId block_id) {
    assert(p->cur_funct && "no current function");
    assert(block_id.id < p->blocks.size() && "invalid block");
    p->cur_block = &p->blocks[block_id.id];
}

NkIrLocalVarId nkir_makeLocalVar(NkIrProg p, nk_type_t type) {
    assert(p->cur_funct && "no current function");
    NkIrLocalVarId id{p->cur_funct->locals.size()};
    p->cur_funct->locals.emplace_back(type);
    return id;
}

NkIrGlobalVarId nkir_makeGlobalVar(NkIrProg p, nk_type_t type) {
    NkIrGlobalVarId id{p->globals.size()};
    p->globals.emplace_back(type);
    return id;
}

NkIrExtVarId nkir_makeExtSym(NkIrProg p, NkIrShObjId so, nkstr name, nk_type_t type) {
    NkIrExtVarId id{p->exsyms.size()};
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
    // TODO assert(index < types::tuple_size(_cur_funct->args_t) && "arg index out of range");
    return {
        .index = index,
        .offset = 0,
        .post_offset = 0,
        .type = 0, // TODO types::tuple_typeAt(_cur_funct->args_t, index),
        .ref_type = NkIrRef_Arg,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeRetRef(NkIrProg p) {
    assert(p->cur_funct && "no current function");
    return {
        .data = {},
        .offset = 0,
        .post_offset = 0,
        .type = 0, // TODO _cur_funct->ret_t,
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

NkIrRef nkir_makeConstRef(NkIrProg p, nk_value_t val) {
    return {
        .data = 0, // TODO val_data(val_copy(val, prog.arena)),
        .offset = 0,
        .post_offset = 0,
        .type = 0, // TODO val_typeof(val),
        .ref_type = NkIrRef_Const,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeRegRef(NkIrProg p, NkIrRegister reg, nk_type_t type) {
    // TODO assert(type->size <= REG_SIZE && "reference type excedes register size");
    return {
        .index = reg,
        .offset = 0,
        .post_offset = 0,
        .type = type,
        .ref_type = NkIrRef_Reg,
        .is_indirect = false,
    };
}

NkIrRef nkir_makeExtVarRef(NkIrProg p, NkIrExtVarId var) {
    return {
        .data = 0, // TODO val_data(val_copy(val, prog.arena)),
        .offset = 0,
        .post_offset = 0,
        .type = 0, // TODO val_typeof(val),
        .ref_type = NkIrRef_Const,
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

NkIrInstr nkir_make_jmpz(NkIrRefPtr cond, NkIrBlockId label) {
    return {{{}, _arg(cond), _arg(label)}, nkir_jmpz};
}

NkIrInstr nkir_make_jmpnz(NkIrRefPtr cond, NkIrBlockId label) {
    return {{{}, _arg(cond), _arg(label)}, nkir_jmpnz};
}

NkIrInstr nkir_make_cast(NkIrRefPtr dst, nk_type_t type, NkIrRefPtr arg) {
    // TODO return {{_arg(dst), _arg(type), _arg(arg)}, nkir_cast};
}

NkIrInstr nkir_make_call(NkIrRefPtr dst, NkIrFunctId funct, NkIrRefPtr args) {
    return {{_arg(dst), _arg(funct), _arg(args)}, nkir_call};
}

NkIrInstr nkir_make_call_ext(NkIrRefPtr dst, NkIrExtFunctId funct, NkIrRefPtr args) {
    return {{_arg(dst), _arg(funct), _arg(args)}, nkir_call};
}

NkIrInstr nkir_make_call_indir(NkIrRefPtr dst, NkIrRefPtr funct, NkIrRefPtr args) {
    return {{_arg(dst), _arg(funct), _arg(args)}, nkir_call};
}

#define U(NAME)                                                  \
    NkIrInstr nkir_make_##NAME(NkIrRefPtr dst, NkIrRefPtr arg) { \
        return {{_arg(dst), _arg(arg), {}}, nkir_##NAME};        \
    }
#define B(NAME)                                                                  \
    NkIrInstr nkir_make_##NAME(NkIrRefPtr dst, NkIrRefPtr lhs, NkIrRefPtr rhs) { \
        return {{_arg(dst), _arg(lhs), _arg(rhs)}, nkir_##NAME};                 \
    }
#include "nk/vm/ir.inl"

void nkir_gen(NkIrProg p, NkIrInstrPtr instr) {
    assert(p->cur_block && "no current block");
    assert(
        instr->arg[0].arg_type != NkIrArg_Ref || instr->arg[0].ref.is_indirect ||
        (instr->arg[0].ref.ref_type != NkIrRef_Const && instr->arg[0].ref.ref_type != NkIrRef_Arg));

    NkIrInstrId id{p->instrs.size()};
    p->instrs.emplace_back(*instr);
    p->cur_block->instrs.emplace_back(id);
}

void nkir_invoke(NkIrProg p, NkIrFunctId fn, nk_value_t ret, nk_value_t args) { // TODO
}

void nkir_inspect(NkIrProg p, NkStringBuilder sb) { // TODO
}

void nkir_inspectRef(NkIrProg p, NkIrRefPtr ref, NkStringBuilder sb) {
    if (ref->ref_type == NkIrRef_None) {
        nksb_printf(sb, "{}");
        return;
    }
    if (ref->is_indirect) {
        nksb_printf(sb, "[");
    }
    switch (ref->ref_type) {
    case NkIrRef_Frame:
        nksb_printf(sb, "$%ul", ref->index);
        break;
    case NkIrRef_Arg:
        nksb_printf(sb, "$arg%ul", ref->index);
        break;
    case NkIrRef_Ret:
        nksb_printf(sb, "$ret");
        break;
    case NkIrRef_Global:
        nksb_printf(sb, "$global%ul", ref->index);
        break;
    case NkIrRef_Const:
        // TODO val_inspect(value_t{ref->data, ref->type}, sb);
        break;
    case NkIrRef_Reg:
        nksb_printf(sb, "$r%c", (char)('a' + ref->index));
        break;
    case NkIrRef_ExtVar:
        nksb_printf(sb, "(%s)", p->exsyms[ref->index].name.c_str());
        break;
    default:
        break;
    }
    if (ref->offset) {
        nksb_printf(sb, "+", ref->offset);
    }
    if (ref->is_indirect) {
        nksb_printf(sb, "]");
    }
    if (ref->post_offset) {
        nksb_printf(sb, "+%ul", ref->post_offset);
    }
    nksb_printf(sb, ":");
    // TODO types::inspect(ref->type, sb);
}
