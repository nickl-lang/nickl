#include "nk/vm/bc.hpp"

#include <cassert>

#include "find_library.hpp"
#include "nk/ds/hashset.hpp"
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

EOpCode s_ir2opcode[] = {
#define X(NAME) CAT(op_, NAME),
#include "nk/vm/ir.inl"
};

void _inspect(Program const &prog, type_t fn, StringBuilder &sb) {
    FunctInfo const &info = *(FunctInfo *)fn->as.fn.closure;

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
            val_inspect(value_t{prog.rodata.data + arg.offset, arg.type}, sb);
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
            sb << ":";
            type_name(arg.type, sb);
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

    HashSet<size_t> funct_ids_to_translate{};
    defer {
        funct_ids_to_translate.deinit();
    };

    auto block_info_ar = allocator.alloc<BlockInfo>(ir_prog.blocks.size);

    Array<Reloc> relocs{};
    defer {
        relocs.deinit();
    };

    while (prog.funct_info.size < ir_prog.functs.size) {
        *prog.funct_info.push() = {};
    }

    auto globals_layout = calcTupleLayout(ir_prog.globals, allocator);
    if (globals_layout.size > prog.globals.capacity) {
        prog.globals.reserve(globals_layout.size);
        std::memset(prog.globals.data, 0, globals_layout.size);

        LOG_DBG("allocated global storage @%p", prog.globals.data);
    }

    while (prog.shobjs.size < ir_prog.shobjs.size) {
        for (size_t i = prog.shobjs.size; i < ir_prog.shobjs.size; i++) {
            ARRAY_SLICE(char, path, MAX_PATH);
            if (!findLibrary(id2s(ir_prog.shobjs[i]), path)) {
                assert(!"failed to find library");
            }
            auto so = openSharedObject(path);
            assert(so && "failed to open a shared object");
            *prog.shobjs.push() = so;
        }
    }

    while (prog.exsyms.size < ir_prog.exsyms.size) {
        for (size_t i = prog.exsyms.size; i < ir_prog.exsyms.size; i++) {
            auto const &exsym = ir_prog.exsyms[i];
            void *sym = resolveSym(prog.shobjs[exsym.so_id], id2s(exsym.name));
            assert(sym && "failed to resolve symbol");
            *prog.exsyms.push() = sym;
        }
    }

    auto const &funct = ir_prog.functs[funct_id.id];

    auto frame_layout = calcTupleLayout(funct.locals, allocator);

    auto &funct_info = prog.funct_info[funct.id] = {
        .prog = &prog,
        .frame_size = frame_layout.size,
        .frame_align = frame_layout.align,
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
                arg.offset += frame_layout.info_ar[ref.value.index].offset;
                break;
            case ir::Ref_Arg:
                arg.offset += type_tuple_offsetAt(funct.args_t, ref.value.index);
                break;
            case ir::Ref_Ret:
                break;
            case ir::Ref_Global:
                arg.offset += globals_layout.info_ar[ref.value.index].offset;
                break;
            case ir::Ref_Const:
                arg.offset += _pushConst({ref.value.data, ref.type});
                break;
            case ir::Ref_Reg:
                arg.offset += ref.value.index * REG_SIZE;
                break;
            case ir::Ref_ExtVar: {
                arg.ref_type = Ref_Abs;
                arg.offset = (size_t)prog.exsyms[ir_arg.as.id];
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
                prog.exsyms[ir_arg.as.id],
                exsym.as.funct.is_variadic);
            break;
        }
        }
    };

    for (auto const &block : ir_prog.blocks.slice(funct.first_block, funct.block_count)) {
        auto &block_info = block_info_ar[block.id] = {};
        block_info.first_instr = prog.instrs.size;

        for (auto const &ir_instr : ir_prog.instrs.slice(block.first_instr, block.instr_count)) {
            uint16_t code = s_ir2opcode[ir_instr.code];

            auto const &arg1 = ir_instr.arg[1];

            switch (ir_instr.code) {
            case ir::ir_call:
                if (arg1.arg_type == ir::Arg_FunctId) {
                    code = op_call_jmp;
                    if (!prog.funct_info[arg1.as.id].prog) {
                        funct_ids_to_translate.insert(arg1.as.id);
                    }
                }
                break;
            case ir::ir_mov:
            case ir::ir_eq:
            case ir::ir_ne:
                if (arg1.as.ref.type->size <= REG_SIZE &&
                    isZeroOrPowerOf2(arg1.as.ref.type->size)) {
                    code += 1 + log2u(arg1.as.ref.type->size);
                }
                break;
#define NUM_X(NAME) case ir::CAT(ir_, NAME):
#define NUM_INT_X(NAME) case ir::CAT(ir_, NAME):
#include "nk/vm/op.inl"
                code += 1 + NUM_TYPE_INDEX(arg1.as.ref.type->as.num.value_type);
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

    for (auto const &id : funct_ids_to_translate) {
        _translate(ir_prog, prog, {id}, allocator);
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

#ifdef ENABLE_LOGGING
    DynamicStringBuilder sb{};
    defer {
        sb.deinit();
    };
#endif // ENABLE_LOGGING
    LOG_INF("bytecode:\n%s", [&]() {
        return prog.inspect(funct_info.funct_t, sb).moveStr(allocator).data;
    }());

    return funct_info.funct_t;
}

} // namespace

void Program::init() {
    *this = {};
}

void Program::deinit() {
    EASY_BLOCK("bc::Program::deinit", profiler::colors::Cyan200)

    for (auto shobj : shobjs) {
        closeSharedObject(shobj);
    }

    exsyms.deinit();
    shobjs.deinit();
    funct_info.deinit();
    rodata.deinit();
    globals.deinit();
    instrs.deinit();
}

StringBuilder &Program::inspect(type_t fn, StringBuilder &sb) const {
    EASY_BLOCK("bc::Program::inspect", profiler::colors::Cyan200)
    _inspect(*this, fn, sb);
    return sb;
}

type_t ProgramBuilder::translate(ir::FunctId funct_id) {
    EASY_BLOCK("bc::translate", profiler::colors::Cyan200)
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
