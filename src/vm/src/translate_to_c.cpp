#include "nk/vm/translate_to_c.hpp"

#include <fstream>
#include <limits>
#include <sstream>

#include "nk/common/hashmap.hpp"
#include "nk/vm/vm.hpp"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::c_compiler)

using stream = std::ostringstream;

struct value_hm_ctx {
    static hash_t hash(value_t key) {
        return hash_array((uint8_t *)&key, (uint8_t *)&key + sizeof(key));
    }

    static bool equal_to(value_t lhs, value_t rhs) {
        return lhs.data == rhs.data && lhs.type == rhs.type;
    }
};

struct WriterCtx {
    HashMap<type_t, string> type_map;
    size_t typedecl_count;

    HashMap<value_t, string, value_hm_ctx> const_map;
    size_t const_count;

    stream types_s;
    stream data_s;
    stream forward_s;
    stream main_s;
};

void _writePreabmle(stream &src) {
    src << "#include <stdint.h>\n\n";
}

void _writeType(WriterCtx &ctx, type_t type, stream &src) {
    auto found_str = ctx.type_map.find(type);
    if (found_str) {
        src << *found_str;
        return;
    }

    stream tmp_s;
    bool is_complex = false;

    switch (type->typeclass_id) {
    case Type_Numeric:
        switch (type->as.num.value_type) {
        case Int8:
            tmp_s << "char";
            break;
        case Int16:
            tmp_s << "int16_t";
            break;
        case Int32:
            tmp_s << "int32_t";
            break;
        case Int64:
            tmp_s << "int64_t";
            break;
        case Uint8:
            tmp_s << "uint8_t";
            break;
        case Uint16:
            tmp_s << "uint16_t";
            break;
        case Uint32:
            tmp_s << "uint32_t";
            break;
        case Uint64:
            tmp_s << "uint64_t";
            break;
        case Float32:
            tmp_s << "float";
            break;
        case Float64:
            tmp_s << "double";
            break;
        default:
            assert(!"unreachable");
            break;
        }
        break;
    case Type_Ptr:
        _writeType(ctx, type->as.ptr.target_type, tmp_s);
        tmp_s << "*";
        break;
    case Type_Void:
        tmp_s << "void";
        break;
    case Type_Tuple: {
        is_complex = true;
        tmp_s << "struct {\n";
        for (size_t i = 0; i < type_tuple_size(type); i++) {
            tmp_s << "  ";
            _writeType(ctx, type_tuple_typeAt(type, i), tmp_s);
            tmp_s << " _" << i << ";\n";
        }
        tmp_s << "}";
        break;
    }
    case Type_Array: {
        is_complex = true;
        tmp_s << "struct { ";
        _writeType(ctx, type_array_elemType(type), tmp_s);
        tmp_s << " _data[" << type_array_size(type) << "]; }";
        break;
    }
    default:
        assert(!"type not implemented");
        break;
    }

    auto tmp_str = tmp_s.str();

    if (is_complex) {
        ctx.types_s << "typedef " << tmp_str << " type" << ctx.typedecl_count << ";\n";
        tmp_str = "type" + std::to_string(ctx.typedecl_count);
        ctx.typedecl_count++;
    }

    auto &type_str = ctx.type_map.insert(type);
    string{tmp_str.data(), tmp_str.size()}.copy(type_str, *_mctx.tmp_allocator);
    src << type_str;
}

void _writeConst(WriterCtx &ctx, value_t val, stream &src, bool is_complex = false) {
    auto found_str = ctx.const_map.find(val);
    if (found_str) {
        src << *found_str;
        return;
    }

    stream tmp_s;

    switch (val_typeclassid(val)) {
    case Type_Numeric: {
        auto value_type = val_typeof(val)->as.num.value_type;
        switch (value_type) {
        case Int8:
            tmp_s << (int)val_as(int8_t, val);
            break;
        case Uint8:
            tmp_s << (unsigned)val_as(uint8_t, val);
            break;
        case Int16:
            tmp_s << val_as(int16_t, val);
            break;
        case Uint16:
            tmp_s << val_as(uint16_t, val);
            break;
        case Int32:
            tmp_s << val_as(int32_t, val);
            break;
        case Uint32:
            tmp_s << val_as(uint32_t, val);
            break;
        case Int64:
            tmp_s << val_as(int64_t, val);
            break;
        case Uint64:
            tmp_s << val_as(uint64_t, val);
            break;
        case Float32:
            tmp_s.precision(std::numeric_limits<float>::max_digits10);
            tmp_s << val_as(float, val);
            break;
        case Float64:
            tmp_s.precision(std::numeric_limits<double>::max_digits10);
            tmp_s << val_as(double, val);
            break;
        default:
            assert(!"unreachable");
            break;
        }
        if (value_type >= Float32) {
            if (val_sizeof(val) == 4) {
                tmp_s << "f";
            }
        } else {
            if (value_type == Uint8 || value_type == Uint16 || value_type == Uint32 ||
                value_type == Uint64) {
                tmp_s << "u";
            }
            if (val_sizeof(val) == 4) {
                tmp_s << "l";
            } else if (val_sizeof(val) == 8) {
                tmp_s << "ll";
            }
        }
        break;
    }
    case Type_Ptr:
        tmp_s << "&";
        _writeConst(ctx, {val_as(void *, val), val_typeof(val)->as.ptr.target_type}, tmp_s, true);
        break;
    case Type_Tuple: {
        is_complex = true;
        tmp_s << "{ ";
        for (size_t i = 0; i < val_tuple_size(val); i++) {
            _writeConst(ctx, val_tuple_at(val, i), tmp_s);
            tmp_s << ", ";
        }
        tmp_s << "}";
        break;
    }
    case Type_Array: {
        is_complex = true;
        tmp_s << "{ ";
        for (size_t i = 0; i < val_array_size(val); i++) {
            _writeConst(ctx, val_array_at(val, i), tmp_s);
            tmp_s << ", ";
        }
        tmp_s << "}";
        break;
    }
    default:
        assert(!"const type not implemented");
        break;
    }

    auto tmp_str = tmp_s.str();

    if (is_complex) {
        _writeType(ctx, val_typeof(val), ctx.data_s);
        ctx.data_s << " const" << ctx.const_count << " = " << tmp_str << ";\n";
        tmp_str = "const" + std::to_string(ctx.const_count);
        ctx.const_count++;
    }

    auto &const_str = ctx.const_map.insert(val);
    string{tmp_str.data(), tmp_str.size()}.copy(const_str, *_mctx.tmp_allocator);
    src << const_str;
}

void _writeFnSig(
    WriterCtx &ctx,
    stream &src,
    string name,
    type_t ret_t,
    type_t args_t,
    bool va = false) {
    _writeType(ctx, ret_t, src);
    src << " " << name << "(";

    for (size_t i = 0; i < type_tuple_size(args_t); i++) {
        if (i) {
            src << ", ";
        }
        _writeType(ctx, type_tuple_typeAt(args_t, i), src);
        src << " arg" << i;
    }
    if (va) {
        src << ", ...";
    }
    src << ")";
}

void _writeProgram(WriterCtx &ctx, ir::Program const &ir) {
    auto &allocator = *_mctx.tmp_allocator;

    auto &src = ctx.main_s;

    _writePreabmle(ctx.types_s);

    auto block_name_by_id = allocator.alloc<string>(ir.blocks.size);

    for (auto const &f : ir.functs) {
        for (auto const &b : ir.blocks.slice(f.first_block, f.block_count)) {
            block_name_by_id[f.first_block + b.id] = b.name;
        }
    }

    for (auto const &sym : ir.exsyms) {
        switch (sym.sym_type) {
        case ir::Sym_Var:
            assert(!"external vars not implemented");
            break;
        case ir::Sym_Funct:
            ctx.forward_s << "extern ";
            _writeFnSig(
                ctx,
                ctx.forward_s,
                id2s(sym.name),
                sym.as.funct.ret_t,
                sym.as.funct.args_t,
                sym.as.funct.is_variadic);
            ctx.forward_s << ";\n";
            break;
        default:
            assert(!"unreachable");
            break;
        }
    }

    for (auto const &f : ir.functs) {
        _writeFnSig(ctx, ctx.forward_s, f.name, f.ret_t, f.args_t);
        ctx.forward_s << ";\n";

        src << "\n";
        _writeFnSig(ctx, src, f.name, f.ret_t, f.args_t);
        src << " {\n\n";

        _writeType(ctx, type_get_array(type_get_numeric(Uint8), REG_SIZE * Reg_Count), src);
        src << " reg;\n";

        for (size_t i = 0; auto type : f.locals) {
            _writeType(ctx, type, src);
            src << " var" << i++ << ";\n";
        }

        if (f.ret_t->typeclass_id != Type_Void) {
            _writeType(ctx, f.ret_t, src);
            src << " ret;\n";
        }

        src << "\n";

        for (auto const &b : ir.blocks.slice(f.first_block, f.block_count)) {
            src << "l_" << b.name << ":\n";

            auto _writeRef = [&](ir::Ref const &ref) {
                if (ref.ref_type == ir::Ref_Const) {
                    _writeConst(ctx, {ref.value.data, ref.type}, src);
                    return;
                }
                src << "*(";
                _writeType(ctx, ref.type, src);
                src << "*)";
                if (ref.post_offset) {
                    src << "((uint8_t*)";
                }
                if (ref.offset) {
                    src << "((uint8_t*)";
                }
                if (!ref.is_indirect) {
                    src << "&";
                }
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
                    assert(!"global ref not implemented");
                    break;
                case ir::Ref_Reg:
                    src << "*((uint8_t*)&reg+" << ref.value.index * REG_SIZE << ")";
                    break;
                case ir::Ref_ExtVar:
                    assert(!"ext var ref not implemented");
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

                if (instr.arg[0].arg_type == ir::Arg_Ref &&
                    instr.arg[0].as.ref.ref_type != ir::Ref_None) {
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
                    src << "goto l_" << block_name_by_id[f.first_block + instr.arg[1].as.id];
                    break;
                case ir::ir_jmpz:
                    src << "if (0 == ";
                    _writeRef(instr.arg[1].as.ref);
                    src << ") { goto l_" << block_name_by_id[f.first_block + instr.arg[2].as.id]
                        << "; }";
                    break;
                case ir::ir_jmpnz:
                    src << "if (";
                    _writeRef(instr.arg[1].as.ref);
                    src << ") { goto l_" << block_name_by_id[f.first_block + instr.arg[2].as.id]
                        << "; }";
                    break;
                case ir::ir_cast:
                    src << "(";
                    assert(
                        instr.arg[1].as.ref.ref_type == ir::Ref_Const &&
                        "type must be known for cast");
                    _writeType(ctx, *(type_t *)instr.arg[1].as.ref.value.data, src);
                    src << ")";
                    _writeRef(instr.arg[2].as.ref);
                    break;
                case ir::ir_call: {
                    switch (instr.arg[1].arg_type) {
                    case ir::Arg_FunctId:
                        src << ir.functs[instr.arg[1].as.id].name;
                        //@Todo Unfinished call compilation
                        break;
                    case ir::Arg_ExtFunctId: {
                        auto &sym = ir.exsyms[instr.arg[1].as.id];
                        src << id2s(sym.name);
                        break;
                    }
                    case ir::Arg_Ref:
                        assert(!"cannot compile ref call");
                        break;
                    default:
                        assert(!"unreachable");
                        break;
                    }
                    src << "(";
                    if (instr.arg[2].as.ref.ref_type != ir::Ref_None) {
                        auto args_t = instr.arg[2].as.ref.type;
                        for (size_t i = 0; i < type_tuple_size(args_t); i++) {
                            if (i) {
                                src << ", ";
                            }
                            src << "(";
                            _writeRef(instr.arg[2].as.ref);
                            src << ")._" << i;
                        }
                    }
                    src << ")";
                    break;
                }
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

void translateToC(string filename, ir::Program const &ir) {
    LOG_TRC(__FUNCTION__)

    WriterCtx ctx{};

    DEFER({
        ctx.type_map.deinit();
        ctx.const_map.deinit();
    })

    _writeProgram(ctx, ir);

    std::ofstream file{std::string{filename.data, filename.size} + ".c"};
    file << ctx.types_s.str() << "\n";
    file << ctx.data_s.str() << "\n";
    file << ctx.forward_s.str();
    file << ctx.main_s.str();
}

} // namespace vm
} // namespace nk
