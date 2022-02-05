#include "nk/vm/bc.hpp"

#include <cassert>
#include <iomanip>
#include <sstream>

#include "nk/common/logger.hpp"
#include "nk/common/profiler.hpp"
#include "nk/vm/interp.hpp"
#include "nk/vm/so_adapter.hpp"

namespace nk {
namespace vm {
namespace bc {

char const *s_op_names[] = {
#define X(NAME) #NAME,
#define XE(NAME, VAR) #NAME " (" #VAR ")",
#include "nk/vm/op.inl"
};

namespace {

LOG_USE_SCOPE(nk::vm)

void _inspect(Program const &prog, std::ostringstream &ss) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    ss << std::hex << std::setfill(' ') << std::left;
    static constexpr std::streamoff c_padding = 8;

    auto _inspectArg = [&](Ref const &arg) {
        if (arg.is_indirect) {
            ss << "[";
        }
        switch (arg.ref_type) {
        case Ref_Frame:
            ss << "frame+";
            break;
        case Ref_Arg:
            ss << "arg+";
            break;
        case Ref_Ret:
            ss << "ret+";
            break;
        case Ref_Global:
            ss << "global+";
            break;
        case Ref_Const:
            ss << "<" << val_inspect(tmp_arena, value_t{prog.rodata.data + arg.offset, arg.type})
               << ">";
            break;
        case Ref_Reg:
            ss << "reg+";
            break;
        case Ref_Instr:
            ss << "instr+";
            break;
        case Ref_Abs:
            ss << "0x";
            break;
        default:
            assert(!"unreachable");
            break;
        }
        if (arg.ref_type != Ref_Const) {
            ss << arg.offset;
        }
        if (arg.is_indirect) {
            ss << "]";
        }
        if (arg.post_offset) {
            ss << "+" << arg.post_offset;
        }
        if (arg.type) {
            ss << ":" << type_name(tmp_arena, arg.type);
        }
    };

    for (auto const &instr : prog.instrs) {
        ss << std::setw(c_padding) << (&instr - prog.instrs.data) * sizeof(Instr);

        if (instr.arg[0].ref_type != Ref_None) {
            _inspectArg(instr.arg[0]);
            ss << " := ";
        }

        ss << s_op_names[instr.code];

        for (size_t i = 1; i < 3; i++) {
            if (instr.arg[i].ref_type != Ref_None) {
                ss << ((i > 1) ? ", " : " ");
                _inspectArg(instr.arg[i]);
            }
        }

        ss << "\n";
    }
}

enum ENumExt {
    Ext_i8,
    Ext_i16,
    Ext_i32,
    Ext_i64,
    Ext_u8,
    Ext_u16,
    Ext_u32,
    Ext_u64,
    Ext_f32,
    Ext_f64,
};

} // namespace

void Program::init() {
    *this = {};
}

void Program::deinit() {
    EASY_BLOCK("vm::Program::deinit", profiler::colors::Red200)

    for (auto shobj : shobjs) {
        closeSharedObject(shobj);
    }

    shobjs.deinit();
    funct_info.deinit();
    rodata.deinit();
    globals.deinit();
    instrs.deinit();
}

string Program::inspect(Allocator &allocator) {
    std::ostringstream ss;
    _inspect(*this, ss);
    auto str = ss.str();

    return copy(string{str.data(), str.size()}, allocator);
}

void Translator::translateFromIr(Program &prog, ir::Program const &ir) {
    EASY_BLOCK("Translator::translateFromIr", profiler::colors::Red200)

    prog.instrs.init(ir.instrs.size);
    prog.funct_info.push(ir.functs.size);

    enum ERelocType {
        Reloc_Funct,
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

    assert(ir.next_funct_id == ir.functs.size && "ill-formed ir");

    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); });

    auto block_info_ar = tmp_arena.alloc<BlockInfo>(ir.blocks.size);

    Array<Reloc> relocs{};
    DEFER({ relocs.deinit(); });

    prog.globals_t = type_get_tuple(tmp_arena, {ir.globals.data, ir.globals.size});
    if (prog.globals_t->size > 0) {
        prog.globals.push(prog.globals_t->size);
        LOG_DBG("allocating global storage: %p", prog.globals.data)

        std::memset(prog.globals.data, 0, prog.globals_t->size);
    }

    prog.shobjs.init(ir.shobjs.size);
    for (auto so_id : ir.shobjs) {
        auto so = prog.shobjs.push() = openSharedObject(id_to_str(so_id));
        assert(so && "failed to open a shared object");
    }

    auto exsyms = tmp_arena.alloc<void *>(ir.exsyms.size);

    for (size_t i = 0; auto const &exsym : ir.exsyms) {
        void *sym = resolveSym(prog.shobjs[exsym.so_id], id_to_str(exsym.name));
        assert(sym && "failed to resolve symbol");
        exsyms[i] = sym;
    }

    for (size_t fi = 0; auto const &funct : ir.functs) {
        assert(funct.next_block_id == funct.next_block_id && "ill-formed ir");

        auto &funct_info = prog.funct_info[funct.id] = {};
        funct_info.prog = &prog;
        funct_info.frame_t = type_get_tuple(tmp_arena, {funct.locals.data, funct.locals.size});
        funct_info.first_instr = prog.instrs.size;

        funct_info.funct_t =
            type_get_fn(funct.ret_t, funct.args_t, fi++, interp_invoke, &funct_info);

        auto _pushConst = [&](value_t val) -> size_t {
            //@Refactor Implement aligned push to array
            uint8_t *ptr = &prog.rodata.push(val_sizeof(val) + val_alignof(val) - 1);
            ptr = (uint8_t *)roundUpSafe((size_t)ptr, val_alignof(val));
            std::memcpy(ptr, val_data(val), val_sizeof(val));
            return ptr - prog.rodata.data;
        };

        auto _compileArg = [&](size_t ii, size_t ai, Ref &arg, ir::Arg const &ir_arg) {
            switch (ir_arg.arg_type) {
            case ir::Arg_None:
                arg.ref_type = Ref_None;
                break;
            case ir::Arg_Ref: {
                auto const &ref = ir_arg.as.ref;
                arg.offset += ref.offset;
                arg.post_offset = ref.post_offset;
                arg.type = ref.type;
                arg.ref_type = (ERefType)ref.ref_type;
                arg.is_indirect = ref.is_indirect;
                switch (ref.ref_type) {
                case ir::Ref_Frame:
                    arg.offset += type_tuple_offset(funct_info.frame_t, ref.value.index);
                    break;
                case ir::Ref_Arg:
                    arg.offset += type_tuple_offset(funct.args_t, ref.value.index);
                    break;
                case ir::Ref_Ret:
                    break;
                case ir::Ref_Global:
                    arg.offset += type_tuple_offset(prog.globals_t, ref.value.index);
                    break;
                case ir::Ref_Const:
                    arg.offset += _pushConst({ref.value.data, ref.type});
                    break;
                case ir::Ref_Reg:
                    arg.offset += ref.value.index * REG_SIZE;
                    break;
                case ir::Ref_ExtVar: {
                    arg.ref_type = Ref_Abs;
                    arg.offset = (size_t)exsyms[ir_arg.as.id];
                    break;
                }
                default:
                    assert(!"unreachable");
                case ir::Ref_None:
                    break;
                }
                break;
            }
            case ir::Arg_BlockId:
                arg.ref_type = Ref_Instr;
                relocs.push() = {
                    .instr_index = ii,
                    .arg = ai,
                    .target_id = ir_arg.as.id,
                    .reloc_type = Reloc_Block,
                };
                break;
            case ir::Arg_FunctId:
                arg.ref_type = Ref_Const;
                relocs.push() = {
                    .instr_index = ii,
                    .arg = ai,
                    .target_id = ir_arg.as.id,
                    .reloc_type = Reloc_Funct,
                };
                break;
            case ir::Arg_ExtFunctId: {
                auto &exsym = ir.exsyms[ir_arg.as.id];
                arg.ref_type = Ref_Const;
                arg.type = type_get_fn_native(
                    exsym.as.funct.ret_t, exsym.as.funct.args_t, 0, exsyms[ir_arg.as.id], nullptr);
                break;
            }
            }
        };

        for (auto const &block : ir.blocks.slice(funct.first_block, funct.block_count)) {
            auto &block_info = block_info_ar[block.id] = {};
            block_info.first_instr = prog.instrs.size;

            for (auto const &ir_instr : ir.instrs.slice(block.first_instr, block.instr_count)) {
                uint16_t code = 0;

                auto numOp = [&](uint16_t op) {
                    auto const &arg0 = ir_instr.arg[0];
                    auto const &arg1 = ir_instr.arg[1];
                    auto const &arg2 = ir_instr.arg[2];

                    assert(arg0.arg_type == ir::Arg_Ref);
                    assert(arg1.arg_type == ir::Arg_Ref);
                    assert(arg2.arg_type == ir::Arg_Ref);

                    assert(arg0.as.ref.type->id == arg1.as.ref.type->id);
                    assert(arg0.as.ref.type->id == arg2.as.ref.type->id);

                    assert(arg0.as.ref.type->typeclass_id == Type_Numeric);

                    code = op + 1 + NUM_TYPE_INDEX(arg0.as.ref.type->as.num.value_type);
                };

                auto numOpInt = [&](uint16_t op) {
                    auto const &arg0 = ir_instr.arg[0];
                    auto const &arg1 = ir_instr.arg[1];
                    auto const &arg2 = ir_instr.arg[2];

                    assert(arg0.arg_type == ir::Arg_Ref);
                    assert(arg1.arg_type == ir::Arg_Ref);
                    assert(arg2.arg_type == ir::Arg_Ref);

                    assert(arg0.as.ref.type->id == arg1.as.ref.type->id);
                    assert(arg0.as.ref.type->id == arg2.as.ref.type->id);

                    assert(arg0.as.ref.type->typeclass_id == Type_Numeric);
                    assert(arg0.as.ref.type->as.num.value_type < Float32);

                    code = op + 1 + NUM_TYPE_INDEX(arg0.as.ref.type->as.num.value_type);
                };

                //@Refactor Remove boilerplate from opcode compilation
                switch (ir_instr.code) {
                case ir::ir_nop:
                    code = op_nop;
                    break;
                case ir::ir_enter:
                    break;
                case ir::ir_leave:
                    break;
                case ir::ir_ret:
                    code = op_ret;
                    break;
                case ir::ir_jmp:
                    code = op_jmp;
                    break;
                case ir::ir_jmpz:
                    code = op_jmpz;
                    break;
                case ir::ir_jmpnz:
                    code = op_jmpnz;
                    break;
                case ir::ir_cast:
                    code = op_cast;
                    break;
                case ir::ir_call:
                    if (ir_instr.arg[1].arg_type == ir::Arg_FunctId) {
                        code = op_call_jmp;
                    } else {
                        code = op_call;
                    }
                    break;
                case ir::ir_mov:
                    code = op_mov;
                    assert(ir_instr.arg[0].arg_type == ir::Arg_Ref);
                    assert(ir_instr.arg[1].arg_type == ir::Arg_Ref);
                    assert(ir_instr.arg[0].as.ref.type->size == ir_instr.arg[1].as.ref.type->size);
                    if (ir_instr.arg[0].as.ref.type->size <= REG_SIZE &&
                        isZeroOrPowerOf2(ir_instr.arg[0].as.ref.type->size)) {
                        code += 1 + log2u(ir_instr.arg[0].as.ref.type->size);
                    }
                    break;
                case ir::ir_lea:
                    code = op_lea;
                    break;
                case ir::ir_neg:
                    code = op_neg;
                    break;
                case ir::ir_compl:
                    code = op_compl;
                    break;
                case ir::ir_not:
                    code = op_not;
                    break;
                case ir::ir_add:
                    numOp(op_add);
                    break;
                case ir::ir_sub:
                    numOp(op_sub);
                    break;
                case ir::ir_mul:
                    numOp(op_mul);
                    break;
                case ir::ir_div:
                    numOp(op_div);
                    break;
                case ir::ir_mod:
                    numOpInt(op_mod);
                    break;
                case ir::ir_bitand:
                    numOpInt(op_bitand);
                    break;
                case ir::ir_bitor:
                    numOpInt(op_bitor);
                    break;
                case ir::ir_xor:
                    numOpInt(op_xor);
                    break;
                case ir::ir_lsh:
                    numOpInt(op_lsh);
                    break;
                case ir::ir_rsh:
                    numOpInt(op_rsh);
                    break;
                case ir::ir_and:
                    numOp(op_and);
                    break;
                case ir::ir_or:
                    numOp(op_or);
                    break;
                case ir::ir_eq:
                    numOp(op_eq);
                    break;
                case ir::ir_ge:
                    numOp(op_ge);
                    break;
                case ir::ir_gt:
                    numOp(op_gt);
                    break;
                case ir::ir_le:
                    numOp(op_le);
                    break;
                case ir::ir_lt:
                    numOp(op_lt);
                    break;
                case ir::ir_ne:
                    numOp(op_ne);
                    break;
                default:
                    assert(!"unreachable");
                    break;
                }

                auto &instr = prog.instrs.push() = {};
                instr.code = code;
                for (size_t ai = 0; ai < 3; ai++) {
                    _compileArg(prog.instrs.size - 1, ai, instr.arg[ai], ir_instr.arg[ai]);
                }
            }
        }
    }

    for (auto const &reloc : relocs) {
        Ref &arg = prog.instrs[reloc.instr_index].arg[reloc.arg];

        switch (reloc.reloc_type) {
        case Reloc_Funct:
            arg.type = prog.funct_info[reloc.target_id].funct_t;
            break;
        case Reloc_Block:
            arg.offset = block_info_ar[reloc.target_id].first_instr * sizeof(Instr);
            break;
        }
    }
}

} // namespace bc
} // namespace vm
} // namespace nk
