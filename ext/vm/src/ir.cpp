#include "nk/vm/ir.hpp"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <sstream>

#include "nk/common/logger.hpp"
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
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto funct_by_id = tmp_arena.alloc<Funct const *>(prog.functs.size);
    auto block_by_id = tmp_arena.alloc<Block const *>(prog.blocks.size);

    for (auto const &funct : prog.functs) {
        funct_by_id[funct.id] = &funct;
    }

    for (auto const &block : prog.blocks) {
        block_by_id[block.id] = &block;
    }

    static constexpr std::streamoff c_comment_padding = 50;

    ss << std::setfill(' ');

    for (auto const &funct : prog.functs) {
        ss << "\nfn " << funct.name << "(";

        for (size_t i = 0; i < funct.args_t->as.tuple.elems.size; i++) {
            if (i) {
                ss << ", ";
            }
            auto arg_t = funct.args_t->as.tuple.elems[i].type;
            ss << "$arg" << i << ":" << type_name(&tmp_arena, arg_t);
        }

        ss << ") -> " << type_name(&tmp_arena, funct.ret_t) << " {\n\n";

        for (auto const &block : prog.blocks.slice(funct.first_block, funct.block_count)) {
            auto const first_p = ss.tellp();
            ss << "%" << block.name << ":";
            auto const second_p = ss.tellp();

            if (block.comment.data) {
                ss << std::setw(c_comment_padding - std::min(c_comment_padding, second_p - first_p))
                   << "; " << block.comment;
            }

            ss << "\n";

            for (auto const &instr : prog.instrs.slice(block.first_instr, block.instr_count)) {
                auto const _inspectRef = [&](Ref const &ref) {
                    if (ref.ref_type == Ref_None) {
                        ss << "{}";
                        return;
                    }
                    if (ref.is_indirect) {
                        ss << "[";
                    }
                    switch (ref.ref_type) {
                    case Ref_Frame:
                        ss << "$" << ref.value.index;
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
                        ss << "<" << val_inspect(&tmp_arena, value_t{ref.value.data, ref.type})
                           << ">";
                        break;
                    default:
                        break;
                    }
                    if (ref.offset) {
                        ss << "+" << ref.offset;
                    }
                    if (ref.is_indirect) {
                        ss << "]";
                    }
                    if (ref.post_offset) {
                        ss << "+" << ref.post_offset;
                    }
                    ss << ":" << type_name(&tmp_arena, ref.type);
                };

                auto const first_p = ss.tellp();

                ss << "  ";

                if (instr.arg[0].arg_type == Arg_Ref) {
                    _inspectRef(instr.arg[0].as.ref);
                    ss << " := ";
                }

                ss << s_ir_names[instr.code];

                for (size_t i = 1; i < 3; i++) {
                    auto &arg = instr.arg[i];
                    if (arg.arg_type != Arg_None) {
                        ss << ((i > 1) ? ", " : " ");
                    }
                    switch (arg.arg_type) {
                    case Arg_Ref: {
                        auto &ref = arg.as.ref;
                        _inspectRef(ref);
                        break;
                    }
                    case Arg_BlockId:
                        ss << "%" << block_by_id[arg.as.id]->name;
                        break;
                    case Arg_FunctId:
                        ss << funct_by_id[arg.as.id]->name;
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
                       << "; " << instr.comment;
                }

                ss << "\n";
            }

            ss << "\n";
        }

        ss << "}\n";
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

void ProgramBuilder::init() {
    m_cur_funct = nullptr;
    m_cur_block = nullptr;

    prog = {};

    prog.entry_point_id = -1ul;

    prog.arena.init();
}

void ProgramBuilder::deinit() {
    m_cur_funct = nullptr;
    m_cur_block = nullptr;

    prog.arena.deinit();

    prog.globals.deinit();

    prog.instrs.deinit();
    prog.blocks.deinit();

    for (auto &funct : prog.functs) {
        funct.locals.deinit();
    }

    prog.functs.deinit();
}

FunctId ProgramBuilder::makeFunct(bool is_entry_point) {
    size_t id{prog.next_funct_id++};
    if (is_entry_point) {
        assert(prog.entry_point_id == -1ul && "cannot have more than one entry point");
        prog.entry_point_id = id;
    }
    return {id};
}

BlockId ProgramBuilder::makeLabel() {
    assert(m_cur_funct && "no current function");
    return {m_cur_funct->next_block_id++};
}

void ProgramBuilder::startFunct(FunctId funct_id, string name, type_t ret_t, type_t args_t) {
    auto &funct = prog.functs.push() = {};

    funct.id = funct_id.id;

    char *str_data = (char *)prog.arena.alloc(name.size);
    std::memcpy(str_data, name.data, name.size);
    funct.name = string{str_data, name.size};

    funct.ret_t = ret_t;
    funct.args_t = args_t;

    funct.first_block = prog.blocks.size;

    m_cur_funct = &funct;
}

void ProgramBuilder::startBlock(BlockId block_id, string name) {
    auto &block = prog.blocks.push() = {};

    block.id = block_id.id;

    char *str_data = (char *)prog.arena.alloc(name.size);
    std::memcpy(str_data, name.data, name.size);
    block.name = string{str_data, name.size};

    block.first_instr = prog.instrs.size;

    m_cur_block = &block;

    m_cur_funct->block_count++;
}

void ProgramBuilder::comment(string str) {
    assert(m_cur_block && "no current block");

    char *str_data = (char *)prog.arena.alloc(str.size);
    std::memcpy(str_data, str.data, str.size);

    if (m_cur_block->instr_count) {
        // TODO simplify this whole expression
        prog.instrs[m_cur_block->first_instr + m_cur_block->instr_count - 1].comment =
            string{str_data, str.size};
    } else {
        m_cur_block->comment = string{str_data, str.size};
    }
}

Local ProgramBuilder::makeLocalVar(type_t type) {
    assert(m_cur_funct && "no current function");
    m_cur_funct->locals.push() = type;
    return {m_cur_funct->locals.size - 1};
}

Global ProgramBuilder::makeGlobalVar(type_t type) {
    prog.globals.push() = type;
    return {prog.globals.size - 1};
}

Ref ProgramBuilder::makeFrameRef(Local var) const {
    assert(m_cur_funct && "no current function");
    return {
        .value = {.index = var.id},
        .offset = 0,
        .post_offset = 0,
        .type = m_cur_funct->locals[var.id],
        .ref_type = Ref_Frame,
        .is_indirect = false};
}

Ref ProgramBuilder::makeArgRef(size_t index) const {
    assert(m_cur_funct && "no current function");
    assert(index < m_cur_funct->args_t->as.tuple.elems.size && "arg index out of range");
    return {
        .value = {.index = index},
        .offset = 0,
        .post_offset = 0,
        .type = m_cur_funct->args_t->as.tuple.elems[index].type,
        .ref_type = Ref_Arg,
        .is_indirect = false};
}

Ref ProgramBuilder::makeRetRef() const {
    assert(m_cur_funct && "no current function");
    return {
        .value = {},
        .offset = 0,
        .post_offset = 0,
        .type = m_cur_funct->ret_t,
        .ref_type = Ref_Ret,
        .is_indirect = false};
}

Ref ProgramBuilder::makeGlobalRef(Global var) const {
    return {
        .value = {.index = var.id},
        .offset = 0,
        .post_offset = 0,
        .type = prog.globals[var.id],
        .ref_type = Ref_Global,
        .is_indirect = false};
}

Ref ProgramBuilder::makeConstRef(value_t val) {
    size_t val_size = val_sizeof(val);
    char *mem = (char *)prog.arena.alloc_aligned(val_size, val_alignof(val));
    std::memcpy(mem, val_data(val), val_size);
    return {
        .value = {val.data = mem},
        .offset = 0,
        .post_offset = 0,
        .type = val_typeof(val),
        .ref_type = Ref_Const,
        .is_indirect = false};
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
    return {{{}, _arg(cond), _arg(label)}, ir_jmpnz, {}};
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
    assert(
        instr.arg[0].arg_type != Arg_Ref || instr.arg[0].as.ref.is_indirect ||
        (instr.arg[0].as.ref.ref_type != Ref_Const && instr.arg[0].as.ref.ref_type != Ref_Arg));

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

} // namespace ir
} // namespace vm
} // namespace nk
