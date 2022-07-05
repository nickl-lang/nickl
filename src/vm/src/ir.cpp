#include "nk/vm/ir.hpp"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>

#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
#include "nk/utils/utils.hpp"
#include "nk/vm/value.hpp"

namespace nk {
namespace vm {
namespace ir {

char const *s_ir_names[] = {
#define X(NAME) #NAME,
#include "nk/vm/ir.inl"
};

namespace {

LOG_USE_SCOPE(nk::vm::ir);

void _inspect(Program const &prog, StringBuilder &sb) {
    StackAllocator arena{};
    defer {
        arena.deinit();
    };

    auto funct_by_id = arena.alloc<Funct const *>(prog.functs.size);
    auto block_by_id = arena.alloc<Block const *>(prog.blocks.size);

    std::memset(funct_by_id, 0, prog.functs.size * sizeof(void *));
    std::memset(block_by_id, 0, prog.blocks.size * sizeof(void *));

    for (auto const &funct : prog.functs) {
        funct_by_id[funct.id] = &funct;
    }

    for (auto const &block : prog.blocks) {
        block_by_id[block.id] = &block;
    }

    for (auto const &funct : prog.functs) {
        sb << "\nfn " << funct.name << "(";

        for (size_t i = 0; i < type_tuple_size(funct.args_t); i++) {
            if (i) {
                sb << ", ";
            }
            sb << "$arg" << i << ":";
            type_name(type_tuple_typeAt(funct.args_t, i), sb);
        }

        sb << ") -> ";
        type_name(funct.ret_t, sb);
        sb << " {\n\n";

        for (auto const &block : prog.blocks.slice(funct.first_block, funct.block_count)) {
            sb << "%" << block.name << ":";

            sb << "\n";

            for (auto const &instr : prog.instrs.slice(block.first_instr, block.instr_count)) {
                auto const _inspectRef = [&](Ref const &ref) {
                    if (ref.ref_type == Ref_None) {
                        sb << "{}";
                        return;
                    }
                    if (ref.is_indirect) {
                        sb << "[";
                    }
                    switch (ref.ref_type) {
                    case Ref_Frame:
                        sb << "$" << ref.value.index;
                        break;
                    case Ref_Arg:
                        sb << "$arg" << ref.value.index;
                        break;
                    case Ref_Ret:
                        sb << "$ret";
                        break;
                    case Ref_Global:
                        sb << "$global" << ref.value.index;
                        break;
                    case Ref_Const:
                        val_inspect(value_t{ref.value.data, ref.type}, sb);
                        break;
                    case Ref_Reg:
                        sb << "$r" << (char)('a' + ref.value.index);
                        break;
                    case Ref_ExtVar:
                        sb << "(" << id2s(prog.exsyms[ref.value.index].name) << ")";
                        break;
                    default:
                        break;
                    }
                    if (ref.offset) {
                        sb << "+" << ref.offset;
                    }
                    if (ref.is_indirect) {
                        sb << "]";
                    }
                    if (ref.post_offset) {
                        sb << "+" << ref.post_offset;
                    }
                    sb << ":";
                    type_name(ref.type, sb);
                };

                sb << "  ";

                if (instr.arg[0].arg_type == Arg_Ref) {
                    _inspectRef(instr.arg[0].as.ref);
                    sb << " := ";
                }

                sb << s_ir_names[instr.code];

                for (size_t i = 1; i < 3; i++) {
                    auto &arg = instr.arg[i];
                    if (arg.arg_type != Arg_None) {
                        sb << ((i > 1) ? ", " : " ");
                    }
                    switch (arg.arg_type) {
                    case Arg_Ref: {
                        auto &ref = arg.as.ref;
                        _inspectRef(ref);
                        break;
                    }
                    case Arg_BlockId:
                        if (arg.as.id < prog.blocks.size && block_by_id[arg.as.id]) {
                            sb << "%" << block_by_id[arg.as.id]->name;
                        } else {
                            sb << "%(null)";
                        }
                        break;
                    case Arg_FunctId:
                        if (arg.as.id < prog.functs.size && funct_by_id[arg.as.id]) {
                            sb << funct_by_id[arg.as.id]->name;
                        } else {
                            sb << "(null)";
                        }
                        break;
                    case Arg_ExtFunctId:
                        sb << "(" << id2s(prog.exsyms[arg.as.id].name) << ")";
                        break;
                    case Arg_None:
                    default:
                        break;
                    }
                }

                sb << "\n";
            }

            sb << "\n";
        }

        sb << "}\n";
    }

    if (prog.exsyms.size) {
        for (auto const &sym : prog.exsyms) {
            sb << "\n" << id2s(prog.shobjs[sym.so_id]) << " " << id2s(sym.name) << ":";
            switch (sym.sym_type) {
            case Sym_Var:
                type_name(sym.as.var.type, sb);
                break;
            case Sym_Funct:
                sb << "fn{";
                type_name(sym.as.funct.args_t, sb);
                if (sym.as.funct.is_variadic) {
                    sb << "...";
                }
                sb << ", ";
                type_name(sym.as.funct.ret_t, sb);
                sb << "}";
                break;
            default:
                assert(!"unreachable");
                break;
            }
        }
        sb << "\n";
    }
}

Arg _arg(Ref const &ref) {
    return {{.ref = ref}, Arg_Ref};
}

Arg _arg(BlockId block) {
    return {{.id = block.id}, Arg_BlockId};
}

Arg _arg(FunctId funct) {
    return {{.id = funct.id}, Arg_FunctId};
}

Arg _arg(ExtFunctId funct) {
    return {{.id = funct.id}, Arg_ExtFunctId};
}

} // namespace

Ref Ref::plus(size_t offset) const {
    return plus(offset, type);
}

Ref Ref::plus(size_t offset, type_t type) const {
    Ref copy{*this};
    if (is_indirect) {
        copy.post_offset += offset;
    } else {
        copy.offset += offset;
    }
    copy.type = type;
    return copy;
}

Ref Ref::deref(type_t type) const {
    assert(this->type->typeclass_id == Type_Ptr);
    Ref copy{*this};
    copy.is_indirect = true;
    copy.type = type;
    return copy;
}

Ref Ref::deref() const {
    assert(this->type->typeclass_id == Type_Ptr);
    Ref copy{*this};
    copy.is_indirect = true;
    copy.type = type->as.ptr.target_type;
    return copy;
}

Ref Ref::as(type_t type) const {
    Ref copy{*this};
    copy.type = type;
    return copy;
}

void Program::init() {
    EASY_BLOCK("ir::Program::init", profiler::colors::Amber200)

    *this = {};

    functs.reserve(10);
    blocks.reserve(10);
    instrs.reserve(100);
}

void Program::deinit() {
    EASY_BLOCK("ir::Program::deinit", profiler::colors::Amber200)

    arena.deinit();

    globals.deinit();
    shobjs.deinit();
    exsyms.deinit();
    instrs.deinit();
    blocks.deinit();
    for (auto &funct : functs) {
        funct.locals.deinit();
    }
    functs.deinit();
}

StringBuilder &Program::inspect(StringBuilder &sb) const {
    _inspect(*this, sb);
    return sb;
}

FunctId ProgramBuilder::makeFunct() {
    LOG_DBG("makeFunct() -> id=%lu", prog.next_funct_id);
    return {prog.next_funct_id++};
}

BlockId ProgramBuilder::makeLabel() {
    assert(_cur_funct && "no current function");
    LOG_DBG("makeLabel() -> id=%lu", _cur_funct->next_block_id);
    return {_cur_funct->next_block_id++};
}

ShObjId ProgramBuilder::makeShObj(Id name) {
    LOG_DBG("makeShObj() -> id=%lu", prog.shobjs.size);
    *prog.shobjs.push() = name;
    return {prog.shobjs.size - 1};
}

void ProgramBuilder::startFunct(FunctId funct_id, string name, type_t ret_t, type_t args_t) {
    EASY_FUNCTION(profiler::colors::Amber200)

    LOG_DBG("startFunct(id=%lu)", funct_id.id);

    auto &funct = *prog.functs.push() = {};

    funct.id = funct_id.id;
    funct.name = name.copy(prog.arena.alloc<char>(name.size));
    funct.ret_t = ret_t;
    funct.args_t = args_t;

    funct.first_block = prog.blocks.size;

    _cur_funct = &funct;
}

void ProgramBuilder::startBlock(BlockId block_id, string name) {
    EASY_FUNCTION(profiler::colors::Amber200)

    LOG_DBG("startBlock(id=%lu)", block_id.id);

    auto &block = *prog.blocks.push() = {};

    block.id = block_id.id;
    block.name = name.copy(prog.arena.alloc<char>(name.size));

    block.first_instr = prog.instrs.size;

    _cur_block = &block;

    _cur_funct->block_count++;
}

Local ProgramBuilder::makeLocalVar(type_t type) {
    assert(_cur_funct && "no current function");
    *_cur_funct->locals.push() = type;
    return {_cur_funct->locals.size - 1};
}

Global ProgramBuilder::makeGlobalVar(type_t type) {
    *prog.globals.push() = type;
    return {prog.globals.size - 1};
}

ExtVarId ProgramBuilder::makeExtVar(ShObjId so, Id name, type_t type) {
    auto &exsym = *prog.exsyms.push() = {};

    exsym.name = name;
    exsym.so_id = so.id;
    exsym.sym_type = Sym_Var;
    exsym.as.var.type = type;

    return {prog.exsyms.size - 1};
}

ExtFunctId ProgramBuilder::makeExtFunct(
    ShObjId so,
    Id name,
    type_t ret_t,
    type_t args_t,
    bool is_variadic) {
    auto &exsym = *prog.exsyms.push() = {};

    exsym.name = name;
    exsym.so_id = so.id;
    exsym.sym_type = Sym_Funct;
    exsym.as.funct.ret_t = ret_t;
    exsym.as.funct.args_t = args_t;
    exsym.as.funct.is_variadic = is_variadic;

    return {prog.exsyms.size - 1};
}

Ref ProgramBuilder::makeFrameRef(Local var) const {
    assert(_cur_funct && "no current function");
    return {
        .value = {.index = var.id},
        .offset = 0,
        .post_offset = 0,
        .type = _cur_funct->locals[var.id],
        .ref_type = Ref_Frame,
        .is_indirect = false,
    };
}

Ref ProgramBuilder::makeArgRef(size_t index) const {
    assert(_cur_funct && "no current function");
    assert(index < type_tuple_size(_cur_funct->args_t) && "arg index out of range");
    return {
        .value = {.index = index},
        .offset = 0,
        .post_offset = 0,
        .type = type_tuple_typeAt(_cur_funct->args_t, index),
        .ref_type = Ref_Arg,
        .is_indirect = false,
    };
}

Ref ProgramBuilder::makeRetRef() const {
    assert(_cur_funct && "no current function");
    return {
        .value = {},
        .offset = 0,
        .post_offset = 0,
        .type = _cur_funct->ret_t,
        .ref_type = Ref_Ret,
        .is_indirect = false,
    };
}

Ref ProgramBuilder::makeGlobalRef(Global var) const {
    return {
        .value = {.index = var.id},
        .offset = 0,
        .post_offset = 0,
        .type = prog.globals[var.id],
        .ref_type = Ref_Global,
        .is_indirect = false,
    };
}

Ref ProgramBuilder::makeConstRef(value_t val) {
    return {
        .value = {.data = val_data(val_copy(val, prog.arena))},
        .offset = 0,
        .post_offset = 0,
        .type = val_typeof(val),
        .ref_type = Ref_Const,
        .is_indirect = false,
    };
}

Ref ProgramBuilder::makeRegRef(ERegister reg, type_t type) const {
    assert(type->size <= REG_SIZE && "reference type excedes register size");
    return {
        .value = {.index = reg},
        .offset = 0,
        .post_offset = 0,
        .type = type,
        .ref_type = Ref_Reg,
        .is_indirect = false,
    };
}

Ref ProgramBuilder::makeExtVarRef(ExtVarId var) const {
    return {
        .value = {.index = var.id},
        .offset = 0,
        .post_offset = 0,
        .type = prog.exsyms[var.id].as.var.type,
        .ref_type = Ref_ExtVar,
        .is_indirect = false,
    };
}

Instr ProgramBuilder::make_nop() {
    return {{}, ir_nop};
}

Instr ProgramBuilder::make_enter() {
    return {{}, ir_enter};
}

Instr ProgramBuilder::make_leave() {
    return {{}, ir_leave};
}

Instr ProgramBuilder::make_ret() {
    return {{}, ir_ret};
}

Instr ProgramBuilder::make_jmp(BlockId label) {
    return {{{}, _arg(label)}, ir_jmp};
}

Instr ProgramBuilder::make_jmpz(Ref const &cond, BlockId label) {
    return {{{}, _arg(cond), _arg(label)}, ir_jmpz};
}

Instr ProgramBuilder::make_jmpnz(Ref const &cond, BlockId label) {
    return {{{}, _arg(cond), _arg(label)}, ir_jmpnz};
}

Instr ProgramBuilder::make_cast(Ref const &dst, Ref const &type, Ref const &arg) {
    return {{_arg(dst), _arg(type), _arg(arg)}, ir_cast};
}

Instr ProgramBuilder::make_call(Ref const &dst, FunctId funct, Ref const &args) {
    return {{_arg(dst), _arg(funct), _arg(args)}, ir_call};
}

Instr ProgramBuilder::make_call(Ref const &dst, Ref const &funct, Ref const &args) {
    return {{_arg(dst), _arg(funct), _arg(args)}, ir_call};
}

Instr ProgramBuilder::make_call(Ref const &dst, ExtFunctId funct, Ref const &args) {
    return {{_arg(dst), _arg(funct), _arg(args)}, ir_call};
}

#define U(NAME)                                                         \
    Instr ProgramBuilder::make_##NAME(Ref const &dst, Ref const &arg) { \
        return {{_arg(dst), _arg(arg), {}}, ir_##NAME};                 \
    }
#define B(NAME)                                                                         \
    Instr ProgramBuilder::make_##NAME(Ref const &dst, Ref const &lhs, Ref const &rhs) { \
        return {{_arg(dst), _arg(lhs), _arg(rhs)}, ir_##NAME};                          \
    }
#include "nk/vm/ir.inl"

void ProgramBuilder::gen(Instr const &instr) {
    EASY_BLOCK("ProgramBuilder::gen", profiler::colors::Amber200)

    assert(_cur_block && "no current block");
    assert(
        instr.arg[0].arg_type != Arg_Ref || instr.arg[0].as.ref.is_indirect ||
        (instr.arg[0].as.ref.ref_type != Ref_Const && instr.arg[0].as.ref.ref_type != Ref_Arg));

    *prog.instrs.push() = instr;
    _cur_block->instr_count++;
}

} // namespace ir
} // namespace vm
} // namespace nk
