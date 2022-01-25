#include "nk/vm/ir.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "nk/common/utils.hpp"

namespace nk {
namespace vm {
namespace ir {

namespace {

static char const *s_ir_names[] = {
#define X(NAME) #NAME,
#include "nk/vm/ir.inl"
};

void _inspect(Program const &prog, std::ostringstream &ss) {
    static constexpr std::streamoff c_comment_padding = 50;

    ss << std::setfill(' ');

    // TODO Either get rid of ss or this allocator
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    for (size_t fi = 0; fi < prog.functs.size; fi++) {
        auto &funct = prog.functs.data[fi];

        ss << "fn " << funct.name.view() << "(";

        for (size_t i = 0; i < funct.args_t->as.tuple.elems.size; i++) {
            if (i > 0) {
                ss << ", ";
            }
            auto arg_t = funct.args_t->as.tuple.elems.data[i].type;
            ss << "$arg" << i << ":" << type_name(&tmp_arena, arg_t).view();
        }

        ss << ") -> " << type_name(&tmp_arena, funct.ret_t).view() << " {\n\n";

        for (size_t bi = funct.first_block; bi < funct.block_count; bi++) {
            auto &block = prog.blocks.data[bi];

            auto const first_p = ss.tellp();
            ss << "%" << block.name.view() << ":";
            auto const second_p = ss.tellp();

            if (block.comment.data) {
                ss << std::setw(c_comment_padding - std::min(c_comment_padding, second_p - first_p))
                   << "; " << block.comment.view();
            }

            ss << "\n";

            for (size_t ii = block.first_instr; ii < block.instr_count; ii++) {
                auto &instr = prog.instrs.data[ii];

                auto const inspect_ref = [&](Ref const &ref) {
                    if (ref.is_indirect) {
                        ss << "[";
                    }
                    switch (ref.ref_type) {
                    case Ref_Frame:
                        break;
                    case Ref_Arg:
                        ss << "$arg" << ref.value.index;
                        break;
                    case Ref_Ret:
                        ss << "$ret";
                        break;
                    case Ref_Global:
                        ss << "$global" << ref.value.index;
                        break;
                    case Ref_Const:
                        // TODO actually inspect the const ref value
                        ss << "<" << ref.value.data << ">";
                        break;
                    default:
                        break;
                    }
                    if (ref.is_indirect) {
                        ss << "]";
                    }
                    if (ref.offset > 0) {
                        ss << "+" << ref.offset;
                    }
                    ss << ":" << type_name(&tmp_arena, ref.type).view();
                };

                auto const first_p = ss.tellp();

                ss << "  ";

                if (instr.arg[0].arg_type == Arg_Ref) {
                    inspect_ref(instr.arg[0].as.ref);
                    ss << " := ";
                }

                ss << s_ir_names[instr.code];

                for (size_t i = 1; i < 3; i++) {
                    auto &arg = instr.arg[i];
                    switch (arg.arg_type) {
                    case Arg_Ref: {
                        ss << ((i > 1) ? ", " : " ");
                        auto &ref = arg.as.ref;
                        inspect_ref(ref);
                        break;
                    }
                    case Arg_BlockId:
                        ss << " %" << prog.blocks.data[arg.as.id].name.view();
                        break;
                    case Arg_FunctId:
                        ss << " " << prog.functs.data[arg.as.id].name.view();
                        break;
                    case Arg_None:
                    default:
                        break;
                    }
                }

                auto const second_p = ss.tellp();

                if (instr.comment.data) {
                    ss << std::setw(
                              c_comment_padding - std::min(c_comment_padding, second_p - first_p))
                       << "; " << instr.comment.view();
                }

                ss << "\n";
            }

            ss << "\n";
        }

        ss << "}\n";
    }
}

} // namespace

void ProgramBuilder::init() {
    m_cur_funct = nullptr;
    m_cur_block = nullptr;

    prog.functs.init();
    prog.blocks.init();
    prog.instrs.init();

    prog.globals.init();

    prog.arena.init();
}

void ProgramBuilder::deinit() {
    m_cur_funct = nullptr;
    m_cur_block = nullptr;

    prog.arena.deinit();

    prog.globals.deinit();

    prog.instrs.deinit();
    prog.blocks.deinit();
    prog.functs.deinit();
}

FunctId ProgramBuilder::makeFunct(string name, type_t ret_t, type_t args_t) {
    auto &funct = prog.functs.push();

    char *str_data = (char *)prog.arena.alloc(name.size);
    std::memcpy(str_data, name.data, name.size);
    funct.name = string{str_data, name.size};

    funct.ret_t = ret_t;
    funct.args_t = args_t;

    return {prog.functs.size - 1};
}

BlockId ProgramBuilder::makeLabel(string name) {
    assert(m_cur_funct && "no current function");

    auto &block = prog.blocks.push();

    block.id = m_cur_funct->block_count++;

    char *str_data = (char *)prog.arena.alloc(name.size);
    std::memcpy(str_data, name.data, name.size);
    block.name = string{str_data, name.size};

    return {prog.blocks.size - 1};
}

void ProgramBuilder::startFunct(FunctId funct) {
    m_cur_funct = &prog.functs.data[funct.id];

    m_cur_funct->first_block = prog.blocks.size;
    m_cur_funct->block_count = 0;
}

void ProgramBuilder::startBlock(BlockId label) {
    m_cur_block = &prog.blocks.data[label.id];

    m_cur_block->first_instr = prog.instrs.size;
    m_cur_block->instr_count = 0;
}

void ProgramBuilder::comment(string str) {
    assert(m_cur_block && "no current block");

    char *str_data = (char *)prog.arena.alloc(str.size);
    std::memcpy(str_data, str.data, str.size);

    if (m_cur_block->instr_count) {
        // TODO simplify this whole expression
        prog.instrs.data[m_cur_block->first_instr + m_cur_block->instr_count - 1].comment =
            string{str_data, str.size};
    } else {
        m_cur_block->comment = string{str_data, str.size};
    }
}

Local ProgramBuilder::makeLocalVar(type_t type) {
    m_cur_funct->locals.push() = type;
    return {m_cur_funct->locals.size - 1};
}

Global ProgramBuilder::makeGlobalVar(type_t type) {
    prog.globals.push() = type;
    return {prog.globals.size - 1};
}

Ref ProgramBuilder::makeFrameRef(Local var) {
    assert(m_cur_funct && "no current function");
    return {
        .value = {.index = var.id},
        .offset = 0,
        .type = m_cur_funct->locals.data[var.id],
        .ref_type = Ref_Frame,
        .is_indirect = false};
}

Ref ProgramBuilder::makeArgRef(size_t index) {
    assert(m_cur_funct && "no current function");
    assert(index < m_cur_funct->args_t->as.tuple.elems.size && "arg index out of range");
    return {
        .value = {.index = index},
        .offset = 0,
        .type = m_cur_funct->args_t->as.tuple.elems.data[index].type,
        .ref_type = Ref_Arg,
        .is_indirect = false};
}

Ref ProgramBuilder::makeRetRef() {
    assert(m_cur_funct && "no current function");
    return {
        .value = {},
        .offset = 0,
        .type = m_cur_funct->ret_t,
        .ref_type = Ref_Ret,
        .is_indirect = false};
}

Ref ProgramBuilder::makeGlobalRef(Global var) {
    return {
        .value = {.index = var.id},
        .offset = 0,
        .type = prog.globals.data[var.id],
        .ref_type = Ref_Global,
        .is_indirect = false};
}

Ref ProgramBuilder::makeConstRef(value_t val) {
    size_t val_size = val_sizeof(val);
    char *mem = (char *)prog.arena.alloc(val_size);
    std::memcpy(mem, val_data(val), val_size);

    return {
        .value = {val.data = mem},
        .offset = 0,
        .type = val_typeof(val),
        .ref_type = Ref_Const,
        .is_indirect = false};
}

Ref ProgramBuilder::refIndirect(Ref const &ref) {
    // TODO maybe rewrite as a ref method?
    Ref copy{ref};
    copy.is_indirect = true;
    return copy;
}

Ref ProgramBuilder::refOffset(Ref const &ref, size_t offset) {
    // TODO maybe rewrite as a ref method?
    Ref copy{ref};
    copy.offset += offset;
    return copy;
}

Instr ProgramBuilder::make_nop() {
    return {{}, ir_nop, {}};
}

Instr ProgramBuilder::make_enter() {
    return {{}, ir_enter, {}};
}

Instr ProgramBuilder::make_leave() {
    return {{}, ir_leave, {}};
}

Instr ProgramBuilder::make_ret() {
    return {{}, ir_ret, {}};
}

Instr ProgramBuilder::make_jmp(BlockId label) {
    return {{{}, _arg(label), {}}, ir_jmp, {}};
}

Instr ProgramBuilder::make_jmpz(Ref const &cond, BlockId label) {
    return {{{}, _arg(cond), _arg(label)}, ir_jmpz, {}};
}

Instr ProgramBuilder::make_jmpnz(Ref const &cond, BlockId label) {
    return {{_arg(cond), _arg(label), {}}, ir_jmpnz, {}};
}

Instr ProgramBuilder::make_cast(Ref const &dst, Ref const &type, Ref const &arg) {
    return {{_arg(dst), _arg(type), _arg(arg)}, ir_cast, {}};
}

Instr ProgramBuilder::make_call(Ref const &dst, FunctId funct, Ref const &args) {
    return {{_arg(dst), _arg(funct), _arg(args)}, ir_call, {}};
}

Instr ProgramBuilder::make_call(Ref const &dst, Ref const &funct, Ref const &args) {
    return {{_arg(dst), _arg(funct), _arg(args)}, ir_call, {}};
}

#define U(NAME)                                                         \
    Instr ProgramBuilder::make_##NAME(Ref const &dst, Ref const &arg) { \
        return {{_arg(dst), _arg(arg), {}}, ir_##NAME, {}};             \
    }
#include "nk/vm/ir.inl"

#define B(NAME)                                                                         \
    Instr ProgramBuilder::make_##NAME(Ref const &dst, Ref const &lhs, Ref const &rhs) { \
        return {{_arg(dst), _arg(lhs), _arg(rhs)}, ir_##NAME, {}};                      \
    }
#include "nk/vm/ir.inl"

void ProgramBuilder::gen(Instr const &instr) {
    assert(m_cur_block && "no current block");

    prog.instrs.push() = instr;
    m_cur_block->instr_count++;
}

string ProgramBuilder::inspect(Allocator *allocator) {
    std::ostringstream ss;
    _inspect(prog, ss);
    auto str = ss.str();

    char *data = (char *)allocator->alloc(str.size());
    std::memcpy(data, str.data(), str.size());

    return string{data, str.size()};
}

Arg ProgramBuilder::_arg(Ref const &ref) {
    return {{.ref = ref}, Arg_Ref};
}

Arg ProgramBuilder::_arg(BlockId block) {
    return {{.id = block.id}, Arg_BlockId};
}

Arg ProgramBuilder::_arg(FunctId funct) {
    return {{.id = funct.id}, Arg_FunctId};
}

} // namespace ir
} // namespace vm
} // namespace nk
