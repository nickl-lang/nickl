#include "translate_to_c.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <ostream>
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
#include "nk/vm/common.h"
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
    NkAllocator arena;

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

    std::set<size_t> ext_syms_forward_declared{};
    std::set<size_t> globals_forward_declared{};
};

void _writePreabmle(std::ostream &src) {
    src << "#include <stdint.h>\n\n";
}

void _writeNumericType(NkNumericValueType value_type, std::ostream &src) {
    switch (value_type) {
    case Int8:
        src << "char";
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
}

void _writeType(WriterCtx &ctx, nktype_t type, std::ostream &src, bool allow_void = false) {
    if (type->size == 0 && allow_void) {
        src << "void";
        return;
    }

    auto found_str = ctx.type_map.find(type);
    if (found_str != ctx.type_map.end()) {
        src << found_str->second;
        return;
    }

    std::ostringstream tmp_s;
    std::ostringstream tmp_s_suf;
    bool is_complex = false;

    switch (type->tclass) {
    case NkType_Numeric:
        _writeNumericType(type->as.num.value_type, tmp_s);
        break;
    case NkType_Ptr:
        _writeType(ctx, type->as.ptr.target_type, tmp_s);
        tmp_s << "*";
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
    case NkType_Fn: {
        is_complex = true;
        auto const ret_t = type->as.fn.ret_t;
        auto const args_t = type->as.fn.args_t;
        auto const is_variadic = type->as.fn.is_variadic;
        _writeType(ctx, ret_t, tmp_s, true);
        tmp_s << " (*";
        tmp_s_suf << ")(";
        for (size_t i = 0; i < args_t->as.tuple.elems.size; i++) {
            if (i) {
                tmp_s_suf << ", ";
            }
            _writeType(ctx, args_t->as.tuple.elems.data[i].type, tmp_s_suf);
        }
        if (is_variadic) {
            tmp_s_suf << ", ...";
        }
        tmp_s_suf << ")";
        break;
    }
    default:
        assert(!"type not implemented");
        break;
    }

    auto type_str = tmp_s.str();

    if (is_complex) {
        ctx.types_s << "typedef " << type_str << " type" << ctx.typedecl_count << tmp_s_suf.str()
                    << ";\n";
        type_str = "type" + std::to_string(ctx.typedecl_count);
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
        case Float32: {
            tmp_s << std::setprecision(std::numeric_limits<float>::max_digits10);
            auto f_val = nkval_as(float, val);
            tmp_s << f_val;
            if (f_val == std::round(f_val)) {
                tmp_s << ".";
            }
            tmp_s << "f";
            break;
        }
        case Float64: {
            tmp_s << std::setprecision(std::numeric_limits<double>::max_digits10);
            auto f_val = nkval_as(double, val);
            tmp_s << f_val;
            if (f_val == std::round(f_val)) {
                tmp_s << ".";
            }
            break;
        }
        default:
            assert(!"unreachable");
            break;
        }
        if (value_type < Float32) {
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
        is_complex = true;
        tmp_s << "& ";
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
    case NkType_Fn: {
        NkIrFunct fn;
        switch (nkval_typeof(val)->as.fn.call_conv) {
        case NkCallConv_Nk: {
            fn = nkval_as(NkIrFunct, val);
            break;
        }
        case NkCallConv_Cdecl: {
            auto it = ctx.ir->closureCode2IrFunct.find(nkval_as(void *, val));
            assert(
                it != ctx.ir->closureCode2IrFunct.end() && "cdecl translation is not implemented");
            fn = it->second;
            break;
        }
        default:
            assert(!"invalid calling convention");
            break;
        }
        tmp_s << fn->name;
        if (ctx.translated.find(fn) == ctx.translated.end()) {
            ctx.to_translate.emplace(fn);
        }
        break;
    }
    default:
        assert(!"unreachable");
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
    _writeType(ctx, ret_t, src, true);
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

// TODO Check if cast is needed
void _writeCast(WriterCtx &ctx, std::ostream &src, nktype_t type) {
    if (type->tclass != NkType_Numeric && type->tclass != NkType_Ptr && type->size) {
        return;
    }

    src << "(";
    _writeType(ctx, type, src);
    src << ")";
}

void _translateFunction(WriterCtx &ctx, NkIrFunct fn) {
    NK_LOG_TRC(__func__);

    ctx.translated.emplace(fn);

    auto &src = ctx.main_s;

    auto fn_t = fn->fn_t;
    auto args_t = fn_t->as.fn.args_t;
    auto ret_t = fn_t->as.fn.ret_t;

    auto const &ir = fn->prog;

    _writeFnSig(ctx, ctx.forward_s, fn->name, ret_t, args_t);
    ctx.forward_s << ";\n";

    src << "\n";
    _writeFnSig(ctx, src, fn->name, ret_t, args_t);
    src << " {\n\n";

    auto u8_t = nkt_get_numeric(Uint8);
    auto reg_t = nkt_get_array(&u8_t, REG_SIZE * NkIrReg_Count);
    _writeType(ctx, &reg_t, src);
    src << " reg={0};\n";

    for (size_t i = 0; auto type : fn->locals) {
        _writeType(ctx, type, src);
        src << " var" << i++ << "={";
        if (type->size) {
            src << "0";
        }
        src << "};\n";
    }

    if (ret_t->size) {
        _writeType(ctx, ret_t, src);
        src << " ret={0};\n";
    }

    src << "\n";

    for (auto bi : fn->blocks) {
        auto const &b = ctx.ir->blocks[bi];

        src << "l_" << b.name << ":\n";

        auto _writeRef = [&](NkIrRef const &ref) {
            if (ref.ref_type == NkIrRef_Const) {
                _writeConst(ctx, nkir_constRefDeref(ir, ref), src);
                return;
            } else if (ref.ref_type == NkIrRef_ExtSym) {
                auto sym = ctx.ir->exsyms[ref.index];
                src << sym.name;
                if (sym.type->tclass == NkType_Fn &&
                    ctx.ext_syms_forward_declared.find(ref.index) ==
                        ctx.ext_syms_forward_declared.end()) {
                    ctx.forward_s << "extern ";
                    _writeFnSig(
                        ctx,
                        ctx.forward_s,
                        sym.name,
                        sym.type->as.fn.ret_t,
                        sym.type->as.fn.args_t,
                        sym.type->as.fn.is_variadic);
                    ctx.forward_s << ";\n";
                    ctx.ext_syms_forward_declared.emplace(ref.index);
                }
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
                src << "& ";
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
                src << "global" << ref.index;
                if (ctx.globals_forward_declared.find(ref.index) ==
                    ctx.globals_forward_declared.end()) {
                    auto const type = ir->globals[ref.index];
                    _writeType(ctx, type, ctx.forward_s);
                    ctx.forward_s << " global" << ref.index << "={";
                    if (type->size) {
                        ctx.forward_s << "0";
                    }
                    ctx.forward_s << "};\n";
                    ctx.globals_forward_declared.emplace(ref.index);
                }
                break;
            case NkIrRef_Reg:
                src << "*((uint8_t*)&reg+" << ref.index * REG_SIZE << ")";
                break;
            case NkIrRef_Const:
            case NkIrRef_ExtSym:
            case NkIrRef_None:
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

            switch (instr.code) {
            case nkir_enter:
            case nkir_leave:
            case nkir_nop:
                continue;
            default:
                break;
            }

            src << "  ";

            if (instr.arg[0].arg_type == NkIrArg_Ref && instr.arg[0].ref.ref_type != NkIrRef_None) {
                _writeRef(instr.arg[0].ref);
                src << " = ";
                _writeCast(ctx, src, instr.arg[0].ref.type);
            }

            switch (instr.code) {
            case nkir_ret:
                src << "return";
                if (ret_t->size) {
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
                assert(
                    instr.arg[1].arg_type == NkIrArg_NumValType &&
                    "numeric value type expected in cast");
                src << "(";
                _writeNumericType((NkNumericValueType)instr.arg[1].id, src);
                src << ")";
                _writeRef(instr.arg[2].ref);
                break;
            case nkir_call: {
                auto fn_t = instr.arg[1].ref.type;
                assert(fn_t->tclass == NkType_Fn);
                src << "(";
                _writeRef(instr.arg[1].ref);
                src << ")";
                src << "(";
                if (instr.arg[2].ref.ref_type != NkIrRef_None) {
                    auto args_t = instr.arg[2].ref.type;
                    for (size_t i = 0; i < args_t->as.tuple.elems.size; i++) {
                        if (i) {
                            src << ", ";
                        }
                        if (i < fn_t->as.fn.args_t->as.tuple.elems.size) {
                            _writeCast(ctx, src, fn_t->as.fn.args_t->as.tuple.elems.data[i].type);
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
                src << "& ";
                _writeRef(instr.arg[1].ref);
                break;

#define UN_OP(NAME, OP)              \
    case CAT(nkir_, NAME):           \
        src << "(";                  \
        src << #OP;                  \
        _writeRef(instr.arg[1].ref); \
        src << ")";                  \
        break;

                UN_OP(neg, -)
                UN_OP(compl, ~)
                UN_OP(not, !)

#undef UN_OP

#define BIN_OP(NAME, OP)             \
    case CAT(nkir_, NAME):           \
        src << "(";                  \
        _writeRef(instr.arg[1].ref); \
        src << " " #OP " ";          \
        _writeRef(instr.arg[2].ref); \
        src << ")";                  \
        break;

                BIN_OP(add, +)
                BIN_OP(sub, -)
                BIN_OP(mul, *)
                BIN_OP(div, /)
                BIN_OP(mod, %)
                BIN_OP(bitand, &)
                BIN_OP(bitor, |)
                BIN_OP(xor, ^)
                BIN_OP(lsh, <<)
                BIN_OP(rsh, >>)
                BIN_OP(and, &&)
                BIN_OP(or, ||)
                BIN_OP(eq, ==)
                BIN_OP(ge, >=)
                BIN_OP(gt, >)
                BIN_OP(le, <=)
                BIN_OP(lt, <)
                BIN_OP(ne, !=)

#undef BIN_OP

            default:
                assert(!"unreachable");
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
