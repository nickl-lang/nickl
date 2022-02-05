#include "nk/vm/c_compiler.hpp"

#include <fstream>

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::c_compiler)

using stream = std::ofstream;

void _writePreabmle(stream &src) {
    src << R"(
#include <stddef.h>
#include <stdint.h>
)";
}

void _writeType(type_t type, stream &src) {
    switch (type->typeclass_id) {
    case Type_Numeric:
        switch (type->as.num.value_type) {
        case Int8:
            src << "int8_t";
            break;
        case Int16:
            src << "int16_t";
            break;
        case Int32:
            src << "int32_t";
            break;
        case Int64:
            src << "int64_t";
            break;
        case Uint8:
            src << "uint8_t";
            break;
        case Uint16:
            src << "uint16_t";
            break;
        case Uint32:
            src << "uint32_t";
            break;
        case Uint64:
            src << "uint64_t";
            break;
        case Float32:
            src << "float";
            break;
        case Float64:
            src << "double";
            break;
        default:
            assert(!"unreachable");
            break;
        }
        break;
    case Type_Ptr:
        _writeType(type->as.ptr.target_type, src);
        src << "*";
        break;
    default:
        src << "UNKNOWN_TYPE";
        break;
    }
}

void _writeProgram(ir::Program const &ir, stream &src) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); });

    _writePreabmle(src);

    auto block_name_by_id = tmp_arena.alloc<string>(ir.blocks.size);

    for (auto const &f : ir.functs) {
        for (auto const &b : ir.blocks.slice(f.first_block, f.block_count)) {
            block_name_by_id[f.first_block + b.id] = b.name;
        }
    }

    for (auto const &f : ir.functs) {
        src << "\n";
        _writeType(f.ret_t, src);
        src << " " << f.name << "(";

        for (size_t i = 0; i < f.args_t->as.tuple.elems.size; i++) {
            if (i) {
                src << ", ";
            }
            _writeType(f.args_t->as.tuple.elems[i].type, src);
            src << " arg" << i;
        }

        src << ") {\n\n";

        src << "struct { uint8_t data[" << REG_SIZE * Reg_Count << "]; } reg;\n";

        for (size_t i = 0; auto type : f.locals) {
            _writeType(type, src);
            src << " var" << i++ << ";\n";
        }

        if (f.ret_t->typeclass_id != Type_Void) {
            _writeType(f.ret_t, src);
            src << " ret;\n";
        }

        src << "\n";

        for (auto const &b : ir.blocks.slice(f.first_block, f.block_count)) {
            src << "l_" << b.name << ":\n";

            auto _writeRef = [&](ir::Ref const &ref) {
                if (ref.ref_type == ir::Ref_Const) {
                    src << val_inspect(tmp_arena, {ref.value.data, ref.type});
                    return;
                }
                src << "*(";
                _writeType(ref.type, src);
                src << "*)";
                if (ref.post_offset) {
                    src << "(";
                }
                if (ref.offset) {
                    src << "((uint8_t*)";
                }
                src << "&";
                switch (ref.ref_type) {
                case ir::Ref_Frame:
                    src << "var" << ref.value.index;
                    break;
                case ir::Ref_Arg:
                    src << "arg" << ref.value.index;
                    break;
                case ir::Ref_Ret:
                    src << "ret";
                    break;
                case ir::Ref_Global:
                    src << "global";
                    break;
                case ir::Ref_Reg:
                    src << "reg";
                    break;
                case ir::Ref_ExtVar:
                    break;
                default:
                    assert(!"unreachable");
                case ir::Ref_None:
                case ir::Ref_Const:
                    break;
                }
                if (ref.offset) {
                    src << "+" << ref.offset << ")";
                }
                if (ref.post_offset) {
                    src << "+" << ref.post_offset << ")";
                }
            };

            for (auto const &instr : ir.instrs.slice(b.first_instr, b.instr_count)) {
                src << "  ";

                if (instr.arg[0].arg_type == ir::Arg_Ref) {
                    _writeRef(instr.arg[0].as.ref);
                    src << " = ";
                }

                switch (instr.code) {
                case ir::ir_ret:
                    src << "return";
                    if (f.ret_t->typeclass_id != Type_Void) {
                        src << " ret";
                    }
                    break;
                case ir::ir_jmp:
                    src << "goto l_" << block_name_by_id[instr.arg[1].as.id];
                    break;
                case ir::ir_jmpz:
                    src << "if (";
                    _writeRef(instr.arg[1].as.ref);
                    src << " == 0) { goto l_" << block_name_by_id[instr.arg[2].as.id] << "; }";
                    break;
                case ir::ir_jmpnz:
                    src << "if (";
                    _writeRef(instr.arg[1].as.ref);
                    src << ") { goto l_" << block_name_by_id[instr.arg[2].as.id] << "; }";
                    break;
                case ir::ir_cast:
                    src << "(";
                    assert(
                        instr.arg[1].as.ref.ref_type == ir::Ref_Const &&
                        "type must be known for cast");
                    _writeType(*(type_t *)instr.arg[1].as.ref.value.data, src);
                    src << ")";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_call:
                    switch (instr.arg[1].arg_type) {
                    case ir::Arg_FunctId:
                        src << ir.functs[instr.arg[1].as.id].name;
                        //@Todo Unfinished call compilation
                        break;
                    case ir::Arg_ExtFunctId:
                        assert(!"cannot compile ext call");
                        break;
                    case ir::Arg_Ref:
                        assert(!"cannot compile ref call");
                        break;
                    default:
                        assert(!"unreachable");
                        break;
                    }
                    src << "()";
                    break;
                case ir::ir_mov:
                    _writeRef(instr.arg[1].as.ref);
                    break;
                case ir::ir_lea:
                    src << "&";
                    _writeRef(instr.arg[1].as.ref);
                    break;
                case ir::ir_neg:
                    src << "-";
                    _writeRef(instr.arg[1].as.ref);
                    break;
                case ir::ir_compl:
                    src << "~";
                    _writeRef(instr.arg[1].as.ref);
                    break;
                case ir::ir_not:
                    src << "!";
                    _writeRef(instr.arg[1].as.ref);
                    break;
                case ir::ir_add:
                    _writeRef(instr.arg[1].as.ref);
                    src << " + ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_sub:
                    _writeRef(instr.arg[1].as.ref);
                    src << " - ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_mul:
                    _writeRef(instr.arg[1].as.ref);
                    src << " * ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_div:
                    _writeRef(instr.arg[1].as.ref);
                    src << " / ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_mod:
                    _writeRef(instr.arg[1].as.ref);
                    src << " % ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_bitand:
                    _writeRef(instr.arg[1].as.ref);
                    src << " & ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_bitor:
                    _writeRef(instr.arg[1].as.ref);
                    src << " | ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_xor:
                    _writeRef(instr.arg[1].as.ref);
                    src << " ^ ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_lsh:
                    _writeRef(instr.arg[1].as.ref);
                    src << " << ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_rsh:
                    _writeRef(instr.arg[1].as.ref);
                    src << " >> ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_and:
                    _writeRef(instr.arg[1].as.ref);
                    src << " && ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_or:
                    _writeRef(instr.arg[1].as.ref);
                    src << " || ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_eq:
                    _writeRef(instr.arg[1].as.ref);
                    src << " == ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_ge:
                    _writeRef(instr.arg[1].as.ref);
                    src << " >= ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_gt:
                    _writeRef(instr.arg[1].as.ref);
                    src << " > ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_le:
                    _writeRef(instr.arg[1].as.ref);
                    src << " <= ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_lt:
                    _writeRef(instr.arg[1].as.ref);
                    src << " < ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_ne:
                    _writeRef(instr.arg[1].as.ref);
                    src << " != ";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                default:
                    assert(!"unreachable");
                case ir::ir_enter:
                case ir::ir_leave:
                case ir::ir_nop:
                    break;
                }

                src << ";\n";
            }

            src << "\n";
        }

        src << "}\n";
    }
}

} // namespace

void CCompiler::compile(string name, ir::Program const &ir) {
    LOG_TRC(__FUNCTION__)

    stream src{std::string{name.data, name.size} + ".c"};
    _writeProgram(ir, src);
}

} // namespace vm
} // namespace nk
