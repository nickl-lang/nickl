#include "translate_to_c.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <vector>

#include "ir_impl.hpp"
#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nk/common/string.hpp"
#include "nk/common/utils.hpp"
#include "nk/vm/ir.h"
#include "nk/vm/value.h"

namespace {

NK_LOG_USE_SCOPE(translate_to_c);

struct nkval_equal_to {
    bool operator()(nkval_t lhs, nkval_t rhs) const noexcept {
        return nkval_data(lhs) == nkval_data(rhs) && nkval_typeid(lhs) == nkval_typeid(rhs);
    }
};

struct nkval_hash {
    hash_t operator()(nkval_t key) const noexcept {
        return hash_array((uint8_t *)&key, (uint8_t *)&key + sizeof(key));
    }
};

struct WriterCtx {
    NkIrProg ir;
    NkAllocator *arena;

    std::ostream &types_s;
    std::ostream &data_s;
    std::ostream &forward_s;
    std::ostream &main_s;

    std::unordered_map<nktype_t, std::string> type_map{};
    size_t typedecl_count{};

    std::unordered_map<nkval_t, std::string, nkval_hash, nkval_equal_to> const_map{};
    size_t const_count{};

    std::set<NkIrFunct> translated{};
    std::stack<NkIrFunct> to_translate{};
};

void _writePreabmle(std::ostream &src) {
    src << "#include <stdint.h>\n\n";
}

void _writeType(WriterCtx &ctx, nktype_t type, std::ostream &src) {
    auto found_str = ctx.type_map.find(type);
    if (found_str != ctx.type_map.end()) {
        src << found_str->second;
        return;
    }

    std::ostringstream tmp_s;
    bool is_complex = false;

    switch (type->typeclass_id) {
    case NkType_Numeric:
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
    case NkType_Ptr:
        tmp_s << "void*";
        break;
    case NkType_Void:
        tmp_s << "void";
        break;
    case NkType_Tuple: {
        is_complex = true;
        tmp_s << "struct {\n";
        for (size_t i = 0; i < type->as.tuple.elems.size; i++) {
            tmp_s << "  ";
            _writeType(ctx, type->as.tuple.elems.data[i].type, tmp_s);
            tmp_s << " _" << i << ";\n";
        }
        tmp_s << "}";
        break;
    }
    case NkType_Array: {
        is_complex = true;
        tmp_s << "struct { ";
        _writeType(ctx, type->as.arr.elem_type, tmp_s);
        tmp_s << " _data[" << type->as.arr.elem_count << "]; }";
        break;
    }
    default:
        assert(!"type not implemented");
        break;
    }

    auto type_str = tmp_s.str();

    if (is_complex) {
        ctx.types_s << "typedef " << type_str << " type" << ctx.typedecl_count << ";\n";
        std::ostringstream s;
        s << "type" << ctx.typedecl_count;
        type_str = s.str();
        ctx.typedecl_count++;
    }

    ctx.type_map.emplace(type, type_str);
    src << type_str;
}

void _writeConst(WriterCtx &ctx, nkval_t val, std::ostream &src, bool is_complex = false) {
    auto found_str = ctx.const_map.find(val);
    if (found_str != ctx.const_map.end()) {
        src << found_str->second;
        return;
    }

    std::ostringstream tmp_s;

    switch (nkval_typeclassid(val)) {
    case NkType_Numeric: {
        auto value_type = nkval_typeof(val)->as.num.value_type;
        switch (value_type) {
        case Int8:
            tmp_s << (int)nkval_as(int8_t, val);
            break;
        case Uint8:
            tmp_s << (unsigned)nkval_as(uint8_t, val);
            break;
        case Int16:
            tmp_s << nkval_as(int16_t, val);
            break;
        case Uint16:
            tmp_s << nkval_as(uint16_t, val);
            break;
        case Int32:
            tmp_s << nkval_as(int32_t, val);
            break;
        case Uint32:
            tmp_s << nkval_as(uint32_t, val);
            break;
        case Int64:
            tmp_s << nkval_as(int64_t, val);
            break;
        case Uint64:
            tmp_s << nkval_as(uint64_t, val);
            break;
        case Float32:
            tmp_s << std::setprecision(std::numeric_limits<float>::max_digits10);
            tmp_s << nkval_as(float, val);
            break;
        case Float64:
            tmp_s << std::setprecision(std::numeric_limits<double>::max_digits10);
            tmp_s << nkval_as(double, val);
            break;
        default:
            assert(!"unreachable");
            break;
        }
        if (value_type >= Float32) {
            if (nkval_sizeof(val) == 4) {
                tmp_s << "f";
            }
        } else {
            if (value_type == Uint8 || value_type == Uint16 || value_type == Uint32 ||
                value_type == Uint64) {
                tmp_s << "u";
            }
            if (nkval_sizeof(val) == 4) {
                tmp_s << "l";
            } else if (nkval_sizeof(val) == 8) {
                tmp_s << "ll";
            }
        }
        break;
    }
    case NkType_Ptr:
        tmp_s << "&";
        _writeConst(
            ctx, {nkval_as(void *, val), nkval_typeof(val)->as.ptr.target_type}, tmp_s, true);
        break;
    case NkType_Tuple: {
        is_complex = true;
        tmp_s << "{ ";
        for (size_t i = 0; i < nkval_tuple_size(val); i++) {
            _writeConst(ctx, nkval_tuple_at(val, i), tmp_s);
            tmp_s << ", ";
        }
        tmp_s << "}";
        break;
    }
    case NkType_Array: {
        is_complex = true;
        tmp_s << "{ ";
        for (size_t i = 0; i < nkval_array_size(val); i++) {
            _writeConst(ctx, nkval_array_at(val, i), tmp_s);
            tmp_s << ", ";
        }
        tmp_s << "}";
        break;
    }
    default:
        assert(!"const type not implemented");
        break;
    }

    auto const_str = tmp_s.str();

    if (is_complex) {
        _writeType(ctx, nkval_typeof(val), ctx.data_s);
        ctx.data_s << " const" << ctx.const_count << " = " << const_str << ";\n";
        std::ostringstream s;
        s << "const" << ctx.const_count;
        const_str = s.str();
        ctx.const_count++;
    }

    ctx.const_map.emplace(val, const_str);
    src << const_str;
}

void _writeFnSig(
    WriterCtx &ctx,
    std::ostream &src,
    std::string const &name,
    nktype_t ret_t,
    nktype_t args_t,
    bool va = false) {
    _writeType(ctx, ret_t, src);
    src << " " << name << "(";

    for (size_t i = 0; i < args_t->as.tuple.elems.size; i++) {
        if (i) {
            src << ", ";
        }
        _writeType(ctx, args_t->as.tuple.elems.data[i].type, src);
        src << " arg" << i;
    }
    if (va) {
        src << ", ...";
    }
    src << ")";
}

void _writeProgram(WriterCtx &ctx, NkIrProg ir) {
    auto &src = ctx.main_s;

    _writePreabmle(ctx.types_s);

    // std::vector<std::string> block_name_by_id;

    // for (auto const &f : ir.functs) {
    //     for (auto const &b : ir.blocks.slice(f->first_block, f->block_count)) {
    //         block_name_by_id[f->first_block + b.id] = b.name;
    //     }
    // }

    for (auto const &sym : ir->exsyms) {
        if (sym.type->typeclass_id == NkType_Fn) {
            NK_LOG_ERR("external vars not implemented");
            std::abort();
        }
        ctx.forward_s << "extern ";
        auto const &fn_t = sym.type->as.fn;
        _writeFnSig(ctx, ctx.forward_s, sym.name, fn_t.ret_t, fn_t.args_t, fn_t.is_variadic);
        ctx.forward_s << ";\n";
    }

    for (auto const &f : ir->functs) {
        auto fn_t = f->fn_t;
        auto args_t = fn_t->as.fn.args_t;
        auto ret_t = fn_t->as.fn.ret_t;

        if (f->name[0] == '#') {
            continue;
        }

        _writeFnSig(ctx, ctx.forward_s, f->name, ret_t, args_t);
        ctx.forward_s << ";\n";

        src << "\n";
        _writeFnSig(ctx, src, f->name, ret_t, args_t);
        src << " {\n\n";

        _writeType(
            ctx,
            nkt_get_array(ctx.arena, nkt_get_numeric(ctx.arena, Uint8), REG_SIZE * NkIrReg_Count),
            src);
        src << " reg;\n";

        for (size_t i = 0; auto type : f->locals) {
            _writeType(ctx, type, src);
            src << " var" << i++ << ";\n";
        }

        if (ret_t->typeclass_id != NkType_Void) {
            _writeType(ctx, ret_t, src);
            src << " ret;\n";
        }

        src << "\n";

        for (auto bi : f->blocks) {
            auto const &b = ir->blocks[bi];

            src << "l_" << b.name << ":\n";

            auto _writeRef = [&](NkIrRef const &ref) {
                if (ref.ref_type == NkIrRef_Const) {
                    _writeConst(ctx, {ref.data, ref.type}, src);
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
                case NkIrRef_Frame:
                    src << "var" << ref.index;
                    break;
                case NkIrRef_Arg:
                    src << "arg" << ref.index;
                    break;
                case NkIrRef_Ret:
                    src << "ret";
                    break;
                case NkIrRef_Global:
                    assert(!"global ref not implemented");
                    break;
                case NkIrRef_Reg:
                    src << "*((uint8_t*)&reg+" << ref.index * REG_SIZE << ")";
                    break;
                case NkIrRef_ExtSym:
                    assert(!"ext sym ref not implemented");
                    break;
                case NkIrRef_Funct:
                    assert(!"funct ref not implemented");
                    break;
                case NkIrRef_None:
                case NkIrRef_Const:
                default:
                    assert(!"unreachable");
                    break;
                }
                if (ref.offset) {
                    src << "+" << ref.offset << ")";
                }
                if (ref.post_offset) {
                    src << "+" << ref.post_offset << ")";
                }
            };

            for (auto ii : b.instrs) {
                auto const &instr = ir->instrs[ii];

                src << "  ";

                if (instr.arg[0].arg_type == NkIrArg_Ref &&
                    instr.arg[0].ref.ref_type != NkIrRef_None) {
                    _writeRef(instr.arg[0].ref);
                    src << " = ";
                }

                switch (instr.code) {
                case nkir_ret:
                    src << "return";
                    if (ret_t->typeclass_id != NkType_Void) {
                        src << " ret";
                    }
                    break;
                case nkir_jmp:
                    src << "goto l_" << ir->blocks[instr.arg[1].id].name;
                    break;
                case nkir_jmpz:
                    src << "if (0 == ";
                    _writeRef(instr.arg[1].ref);
                    src << ") { goto l_" << ir->blocks[instr.arg[2].id].name << "; }";
                    break;
                case nkir_jmpnz:
                    src << "if (";
                    _writeRef(instr.arg[1].ref);
                    src << ") { goto l_" << ir->blocks[instr.arg[2].id].name << "; }";
                    break;
                case nkir_cast:
                    src << "(";
                    assert(
                        instr.arg[1].ref.ref_type == NkIrRef_Const &&
                        "type must be known for cast");
                    _writeType(ctx, *(nktype_t *)instr.arg[1].ref.data, src);
                    src << ")";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_call: {
                    // TODO Call translation not implemented
                    // switch (instr.arg[1].arg_type) {
                    // case NkIrArg_FunctId:
                    //    src << ir->functs[instr.arg[1].id]->name;
                    //    //@Todo Unfinished call compilation
                    //    break;
                    // case NkIrArg_ExtFunctId: {
                    //    auto &sym = ir.exsyms[instr.arg[1].id];
                    //    src << sym.name;
                    //    break;
                    //}
                    // case NkIrArg_Ref:
                    //    assert(!"cannot compile ref call");
                    //    break;
                    // default:
                    //    assert(!"unreachable");
                    //    break;
                    //}
                    src << "(";
                    if (instr.arg[2].ref.ref_type != NkIrRef_None) {
                        auto args_t = instr.arg[2].ref.type;
                        for (size_t i = 0; i < args_t->as.tuple.elems.size; i++) {
                            if (i) {
                                src << ", ";
                            }
                            src << "(";
                            _writeRef(instr.arg[2].ref);
                            src << ")._" << i;
                        }
                    }
                    src << ")";
                    break;
                }
                case nkir_mov:
                    _writeRef(instr.arg[1].ref);
                    break;
                case nkir_lea:
                    src << "&";
                    _writeRef(instr.arg[1].ref);
                    break;
                case nkir_neg:
                    src << "-";
                    _writeRef(instr.arg[1].ref);
                    break;
                case nkir_compl:
                    src << "~";
                    _writeRef(instr.arg[1].ref);
                    break;
                case nkir_not:
                    src << "!";
                    _writeRef(instr.arg[1].ref);
                    break;
                case nkir_add:
                    _writeRef(instr.arg[1].ref);
                    src << " + ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_sub:
                    _writeRef(instr.arg[1].ref);
                    src << " - ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_mul:
                    _writeRef(instr.arg[1].ref);
                    src << " * ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_div:
                    _writeRef(instr.arg[1].ref);
                    src << " / ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_mod:
                    _writeRef(instr.arg[1].ref);
                    src << " % ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_bitand:
                    _writeRef(instr.arg[1].ref);
                    src << " & ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_bitor:
                    _writeRef(instr.arg[1].ref);
                    src << " | ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_xor:
                    _writeRef(instr.arg[1].ref);
                    src << " ^ ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_lsh:
                    _writeRef(instr.arg[1].ref);
                    src << " << ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_rsh:
                    _writeRef(instr.arg[1].ref);
                    src << " >> ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_and:
                    _writeRef(instr.arg[1].ref);
                    src << " && ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_or:
                    _writeRef(instr.arg[1].ref);
                    src << " || ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_eq:
                    _writeRef(instr.arg[1].ref);
                    src << " == ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_ge:
                    _writeRef(instr.arg[1].ref);
                    src << " >= ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_gt:
                    _writeRef(instr.arg[1].ref);
                    src << " > ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_le:
                    _writeRef(instr.arg[1].ref);
                    src << " <= ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_lt:
                    _writeRef(instr.arg[1].ref);
                    src << " < ";
                    _writeRef(instr.arg[2].ref);
                    break;
                case nkir_ne:
                    _writeRef(instr.arg[1].ref);
                    src << " != ";
                    _writeRef(instr.arg[2].ref);
                    break;
                default:
                    assert(!"unreachable");
                case nkir_enter:
                case nkir_leave:
                case nkir_nop:
                    break;
                }

                src << ";\n";
            }

            src << "\n";
        }

        src << "}\n";
    }
}

void _translateFunction(WriterCtx &ctx, NkIrFunct fn) {
    NK_LOG_TRC(__func__);

    auto &src = ctx.main_s;

    auto fn_t = fn->fn_t;
    auto args_t = fn_t->as.fn.args_t;
    auto ret_t = fn_t->as.fn.ret_t;

    _writeFnSig(ctx, ctx.forward_s, fn->name, ret_t, args_t);
    ctx.forward_s << ";\n";

    src << "\n";
    _writeFnSig(ctx, src, fn->name, ret_t, args_t);
    src << " {\n\n";

    _writeType(
        ctx,
        nkt_get_array(ctx.arena, nkt_get_numeric(ctx.arena, Uint8), REG_SIZE * NkIrReg_Count),
        src);
    src << " reg;\n";

    for (size_t i = 0; auto type : fn->locals) {
        _writeType(ctx, type, src);
        src << " var" << i++ << ";\n";
    }

    if (ret_t->typeclass_id != NkType_Void) {
        _writeType(ctx, ret_t, src);
        src << " ret;\n";
    }

    src << "\n";

    for (auto bi : fn->blocks) {
        auto const &b = ctx.ir->blocks[bi];

        src << "l_" << b.name << ":\n";

        auto _writeRef = [&](NkIrRef const &ref) {
            if (ref.ref_type == NkIrRef_Const) {
                _writeConst(ctx, {ref.data, ref.type}, src);
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
            case NkIrRef_Frame:
                src << "var" << ref.index;
                break;
            case NkIrRef_Arg:
                src << "arg" << ref.index;
                break;
            case NkIrRef_Ret:
                src << "ret";
                break;
            case NkIrRef_Global:
                assert(!"global ref not implemented");
                break;
            case NkIrRef_Reg:
                src << "*((uint8_t*)&reg+" << ref.index * REG_SIZE << ")";
                break;
            case NkIrRef_ExtSym:
                assert(!"ext sym ref not implemented");
                break;
            case NkIrRef_Funct:
                assert(!"funct ref not implemented");
                break;
            case NkIrRef_None:
            case NkIrRef_Const:
            default:
                assert(!"unreachable");
                break;
            }
            if (ref.offset) {
                src << "+" << ref.offset << ")";
            }
            if (ref.post_offset) {
                src << "+" << ref.post_offset << ")";
            }
        };

        for (auto ii : b.instrs) {
            auto const &instr = ctx.ir->instrs[ii];

            src << "  ";

            if (instr.arg[0].arg_type == NkIrArg_Ref && instr.arg[0].ref.ref_type != NkIrRef_None) {
                _writeRef(instr.arg[0].ref);
                src << " = ";
            }

            switch (instr.code) {
            case nkir_ret:
                src << "return";
                if (ret_t->typeclass_id != NkType_Void) {
                    src << " ret";
                }
                break;
            case nkir_jmp:
                src << "goto l_" << ctx.ir->blocks[instr.arg[1].id].name;
                break;
            case nkir_jmpz:
                src << "if (0 == ";
                _writeRef(instr.arg[1].ref);
                src << ") { goto l_" << ctx.ir->blocks[instr.arg[2].id].name << "; }";
                break;
            case nkir_jmpnz:
                src << "if (";
                _writeRef(instr.arg[1].ref);
                src << ") { goto l_" << ctx.ir->blocks[instr.arg[2].id].name << "; }";
                break;
            case nkir_cast:
                src << "(";
                assert(instr.arg[1].ref.ref_type == NkIrRef_Const && "type must be known for cast");
                _writeType(ctx, *(nktype_t *)instr.arg[1].ref.data, src);
                src << ")";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_call: {
                // TODO Call translation not implemented
                // switch (instr.arg[1].arg_type) {
                // case NkIrArg_FunctId:
                //    src << ctx.ir->functs[instr.arg[1].id]->name;
                //    //@Todo Unfinished call compilation
                //    break;
                // case NkIrArg_ExtFunctId: {
                //    auto &sym = ir.exsyms[instr.arg[1].id];
                //    src << sym.name;
                //    break;
                //}
                // case NkIrArg_Ref:
                //    assert(!"cannot compile ref call");
                //    break;
                // default:
                //    assert(!"unreachable");
                //    break;
                //}
                src << "(";
                if (instr.arg[2].ref.ref_type != NkIrRef_None) {
                    auto args_t = instr.arg[2].ref.type;
                    for (size_t i = 0; i < args_t->as.tuple.elems.size; i++) {
                        if (i) {
                            src << ", ";
                        }
                        src << "(";
                        _writeRef(instr.arg[2].ref);
                        src << ")._" << i;
                    }
                }
                src << ")";
                break;
            }
            case nkir_mov:
                _writeRef(instr.arg[1].ref);
                break;
            case nkir_lea:
                src << "&";
                _writeRef(instr.arg[1].ref);
                break;
            case nkir_neg:
                src << "-";
                _writeRef(instr.arg[1].ref);
                break;
            case nkir_compl:
                src << "~";
                _writeRef(instr.arg[1].ref);
                break;
            case nkir_not:
                src << "!";
                _writeRef(instr.arg[1].ref);
                break;
            case nkir_add:
                _writeRef(instr.arg[1].ref);
                src << " + ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_sub:
                _writeRef(instr.arg[1].ref);
                src << " - ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_mul:
                _writeRef(instr.arg[1].ref);
                src << " * ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_div:
                _writeRef(instr.arg[1].ref);
                src << " / ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_mod:
                _writeRef(instr.arg[1].ref);
                src << " % ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_bitand:
                _writeRef(instr.arg[1].ref);
                src << " & ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_bitor:
                _writeRef(instr.arg[1].ref);
                src << " | ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_xor:
                _writeRef(instr.arg[1].ref);
                src << " ^ ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_lsh:
                _writeRef(instr.arg[1].ref);
                src << " << ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_rsh:
                _writeRef(instr.arg[1].ref);
                src << " >> ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_and:
                _writeRef(instr.arg[1].ref);
                src << " && ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_or:
                _writeRef(instr.arg[1].ref);
                src << " || ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_eq:
                _writeRef(instr.arg[1].ref);
                src << " == ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_ge:
                _writeRef(instr.arg[1].ref);
                src << " >= ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_gt:
                _writeRef(instr.arg[1].ref);
                src << " > ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_le:
                _writeRef(instr.arg[1].ref);
                src << " <= ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_lt:
                _writeRef(instr.arg[1].ref);
                src << " < ";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_ne:
                _writeRef(instr.arg[1].ref);
                src << " != ";
                _writeRef(instr.arg[2].ref);
                break;
            default:
                assert(!"unreachable");
            case nkir_enter:
            case nkir_leave:
            case nkir_nop:
                break;
            }

            src << ";\n";
        }

        src << "\n";
    }

    src << "}\n";
}

} // namespace

void nkir_translateToC(NkIrProg ir, NkIrFunct entry_point, std::ostream &src) {
    EASY_FUNCTION(::profiler::colors::Amber200);
    NK_LOG_TRC(__func__);

    std::ostringstream types_s;
    std::ostringstream data_s;
    std::ostringstream forward_s;
    std::ostringstream main_s;

    WriterCtx ctx{
        .ir = ir,
        .arena = nk_create_arena(),

        .types_s = types_s,
        .data_s = data_s,
        .forward_s = forward_s,
        .main_s = main_s,
    };

    defer {
        nk_free_arena(ctx.arena);
    };

    _writePreabmle(ctx.types_s);

    _translateFunction(ctx, entry_point);

    while (!ctx.to_translate.empty()) {
        auto fn = ctx.to_translate.top();
        ctx.to_translate.pop();
        _translateFunction(ctx, fn);
    }

    src << types_s.str() << "\n"   //
        << data_s.str() << "\n"    //
        << forward_s.str() << "\n" //
        << main_s.str();           //
}
