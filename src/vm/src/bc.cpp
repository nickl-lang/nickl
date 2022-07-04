#include "nk/vm/bc.hpp"

#include <cassert>

#include "find_library.hpp"
#include "nk/ds/hashmap.hpp"
#include "nk/str/dynamic_string_builder.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
#include "nk/vm/interp.hpp"
#include "so_adapter.hpp"

namespace nk {
namespace vm {
namespace bc {

char const *s_op_names[] = {
#define X(NAME) #NAME,
#define XE(NAME, VAR) #NAME " (" #VAR ")",
#include "nk/vm/op.inl"
};

namespace {

LOG_USE_SCOPE(nk::vm::bc);

void _inspect(Program const &prog, type_t fn, DynamicStringBuilder &sb) {
    FunctInfo const &info = *(FunctInfo *)fn->as.fn.closure;

    //@Performance StackAllocator in _inspect
    StackAllocator arena{};
    defer {
        arena.deinit();
    };

    auto _inspectArg = [&](Ref const &arg) {
        if (arg.is_indirect) {
            sb << "[";
        }
        switch (arg.ref_type) {
        case Ref_Frame:
            sb << "frame+";
            break;
        case Ref_Arg:
            sb << "arg+";
            break;
        case Ref_Ret:
            sb << "ret+";
            break;
        case Ref_Global:
            sb << "global+";
            break;
        case Ref_Const:
            sb << val_inspect(value_t{prog.rodata.data + arg.offset, arg.type}, arena);
            break;
        case Ref_Reg:
            sb << "reg+";
            break;
        case Ref_Instr:
            sb << "instr+";
            break;
        case Ref_Abs:
            sb << "0x";
            break;
        default:
            assert(!"unreachable");
            break;
        }
        if (arg.ref_type != Ref_Const) {
            sb.printf("%zx", arg.offset);
        }
        if (arg.is_indirect) {
            sb << "]";
        }
        if (arg.post_offset) {
            sb.printf("+%zx", arg.post_offset);
        }
        if (arg.type) {
            sb << ":" << type_name(arg.type, arena);
        }
    };

    for (auto const &instr : prog.instrs.slice(info.first_instr, info.instr_count)) {
        sb.printf("%zx\t", (&instr - prog.instrs.data) * sizeof(Instr));

        if (instr.arg[0].ref_type != Ref_None) {
            _inspectArg(instr.arg[0]);
            sb << " := ";
        }

        sb << s_op_names[instr.code];

        for (size_t i = 1; i < 3; i++) {
            if (instr.arg[i].ref_type != Ref_None) {
                sb << ((i > 1) ? ", " : " ");
                _inspectArg(instr.arg[i]);
            }
        }

        sb << "\n";
    }
}

type_t _translate(ir::Program &ir_prog, Program &prog, ir::FunctId funct_id, Allocator &allocator) {
    //@Robustness Using HashMap as a set
    HashMap<size_t, uint8_t> funct_ids_to_translate_set{};
    defer {
        funct_ids_to_translate_set.deinit();
    };

    while (prog.funct_info.size < ir_prog.functs.size) {
        *prog.funct_info.push() = {};
    }

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

    auto block_info_ar = allocator.alloc<BlockInfo>(ir_prog.blocks.size);

    Array<Reloc> relocs{};
    defer {
        relocs.deinit();
    };

    //@Robustness Getting tuple for all globals combinations
    type_t const globals_t = type_get_tuple(ir_prog.globals);
    if (globals_t->size > 0) {
        //@Robustness Clearing and pusing globals multiple times
        prog.globals.clear();
        prog.globals.push(globals_t->size);
        LOG_DBG("allocating global storage: %p", prog.globals.data);

        std::memset(prog.globals.data, 0, globals_t->size);
    }

    prog.shobjs.reserve(ir_prog.shobjs.size);
    for (auto so_id : ir_prog.shobjs) {
        ARRAY_SLICE(char, path, MAX_PATH);
        if (!findLibrary(id2s(so_id), path)) {
            assert(!"failed to find library");
        }
        auto so = *prog.shobjs.push() = openSharedObject({path.data, path.size});
        assert(so && "failed to open a shared object");
    }

    auto exsyms = allocator.alloc<void *>(ir_prog.exsyms.size);

    for (size_t i = 0; auto const &exsym : ir_prog.exsyms) {
        void *sym = resolveSym(prog.shobjs[exsym.so_id], id2s(exsym.name));
        assert(sym && "failed to resolve symbol");
        exsyms[i] = sym;
    }

    auto const &funct = ir_prog.functs[funct_id.id];

    auto &funct_info = prog.funct_info[funct.id] = {
        .prog = &prog,
        .frame_t = type_get_tuple({funct.locals.data, funct.locals.size}),
        .first_instr = prog.instrs.size,
        .instr_count = 0,
        .funct_t = nullptr,
    };

    funct_info.funct_t =
        type_get_fn(funct.ret_t, funct.args_t, funct_id.id, interp_invoke, &funct_info);

    auto _pushConst = [&](value_t val) -> size_t {
        uint8_t *mem = prog.rodata.push_aligned(val_alignof(val), val_sizeof(val));
        val_copy(val, mem);
        return mem - prog.rodata.data;
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
                arg.offset += type_tuple_offsetAt(funct_info.frame_t, ref.value.index);
                break;
            case ir::Ref_Arg:
                arg.offset += type_tuple_offsetAt(funct.args_t, ref.value.index);
                break;
            case ir::Ref_Ret:
                break;
            case ir::Ref_Global:
                arg.offset += type_tuple_offsetAt(globals_t, ref.value.index);
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
            *relocs.push() = {
                .instr_index = ii,
                .arg = ai,
                .target_id = ir_arg.as.id,
                .reloc_type = Reloc_Block,
            };
            break;
        case ir::Arg_FunctId:
            arg.ref_type = Ref_Const;
            *relocs.push() = {
                .instr_index = ii,
                .arg = ai,
                .target_id = ir_arg.as.id,
                .reloc_type = Reloc_Funct,
            };
            break;
        case ir::Arg_ExtFunctId: {
            auto &exsym = ir_prog.exsyms[ir_arg.as.id];
            arg.ref_type = Ref_Const;
            arg.type = type_get_fn_native(
                exsym.as.funct.ret_t,
                exsym.as.funct.args_t,
                0,
                exsyms[ir_arg.as.id],
                exsym.as.funct.is_variadic);
            break;
        }
        }
    };

    for (auto const &block : ir_prog.blocks.slice(funct.first_block, funct.block_count)) {
        auto &block_info = block_info_ar[block.id] = {};
        block_info.first_instr = prog.instrs.size;

        for (auto const &ir_instr : ir_prog.instrs.slice(block.first_instr, block.instr_count)) {
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
                code = op_enter;
                break;
            case ir::ir_leave:
                code = op_leave;
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
                    if (!prog.funct_info[ir_instr.arg[1].as.id].prog) {
                        funct_ids_to_translate_set.insert(ir_instr.arg[1].as.id, 0);
                    }
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
            case ir::ir_eq:
                code = op_eq;
                assert(ir_instr.arg[0].arg_type == ir::Arg_Ref);
                assert(ir_instr.arg[0].as.ref.type->size == 1);
                assert(ir_instr.arg[1].arg_type == ir::Arg_Ref);
                assert(ir_instr.arg[2].arg_type == ir::Arg_Ref);
                assert(ir_instr.arg[1].as.ref.type->size == ir_instr.arg[2].as.ref.type->size);
                if (ir_instr.arg[1].as.ref.type->size <= REG_SIZE &&
                    isZeroOrPowerOf2(ir_instr.arg[1].as.ref.type->size)) {
                    code += 1 + log2u(ir_instr.arg[1].as.ref.type->size);
                }
                break;
            case ir::ir_ne:
                code = op_ne;
                assert(ir_instr.arg[0].arg_type == ir::Arg_Ref);
                assert(ir_instr.arg[0].as.ref.type->size == 1);
                assert(ir_instr.arg[1].arg_type == ir::Arg_Ref);
                assert(ir_instr.arg[2].arg_type == ir::Arg_Ref);
                assert(ir_instr.arg[1].as.ref.type->size == ir_instr.arg[2].as.ref.type->size);
                if (ir_instr.arg[1].as.ref.type->size <= REG_SIZE &&
                    isZeroOrPowerOf2(ir_instr.arg[1].as.ref.type->size)) {
                    code += 1 + log2u(ir_instr.arg[1].as.ref.type->size);
                }
                break;
#define NUM_X(NAME)       \
    case ir::ir_##NAME:   \
        numOp(op_##NAME); \
        break;
#include "nk/vm/op.inl"
#define NUM_INT_X(NAME)      \
    case ir::ir_##NAME:      \
        numOpInt(op_##NAME); \
        break;
#include "nk/vm/op.inl"
            default:
                assert(!"unreachable");
                break;
            }

            auto &instr = *prog.instrs.push() = {};
            funct_info.instr_count++;
            instr.code = code;
            for (size_t ai = 0; ai < 3; ai++) {
                _compileArg(prog.instrs.size - 1, ai, instr.arg[ai], ir_instr.arg[ai]);
            }
        }
    }

    for (size_t i = 0; i < funct_ids_to_translate_set.capacity; i++) {
        auto const &entry = funct_ids_to_translate_set.entries[i];
        if (!entry.isValid()) {
            continue;
        }
        _translate(ir_prog, prog, {entry.key}, allocator);
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

    //@Todo Print bytecode only for the funtion being translated
    LOG_INF("bytecode:\n%s", [&]() {
        //@Robustness Refactor debug printing
        return (DynamicStringBuilder{} << prog.inspect(funct_info.funct_t, allocator))
            .moveStr(allocator)
            .data;
    }());

    return funct_info.funct_t;
}

} // namespace

void Program::init() {
    *this = {};
}

void Program::deinit() {
    EASY_BLOCK("bc::Program::deinit", profiler::colors::Red200)

    for (auto shobj : shobjs) {
        closeSharedObject(shobj);
    }

    shobjs.deinit();
    funct_info.deinit();
    rodata.deinit();
    globals.deinit();
    instrs.deinit();
}

string Program::inspect(type_t fn, Allocator &allocator) const {
    DynamicStringBuilder sb{};
    _inspect(*this, fn, sb);
    return sb.moveStr(allocator);
}

type_t ProgramBuilder::translate(ir::FunctId funct_id) {
    EASY_FUNCTION(profiler::colors::Red200)
    LOG_TRC(__func__);

    StackAllocator arena{};
    defer {
        arena.deinit();
    };

    return _translate(ir_prog, prog, funct_id, arena);
}

} // namespace bc
} // namespace vm
} // namespace nk
