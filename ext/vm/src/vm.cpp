#include "nk/vm/vm.hpp"

#include <cassert>
#include <iomanip>
#include <sstream>

#include "nk/common/logger.hpp"
#include "nk/vm/interp.hpp"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm)

// TODO Duplicate s_ir_names in vm
static char const *s_ir_names[] = {
#define X(NAME) #NAME,
#include "nk/vm/ir.inl"
};

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
            ss << "frame";
            break;
        case Ref_Arg:
            ss << "arg";
            break;
        case Ref_Ret:
            ss << "ret";
            break;
        case Ref_Global:
            ss << "global";
            break;
        case Ref_Const:
            ss << "<" << val_inspect(&tmp_arena, value_t{prog.rodata.data + arg.offset, arg.type})
               << ">";
            break;
        case Ref_Instr:
            ss << "instr";
            break;
        default:
            assert(!"unreachable");
            break;
        }
        if (arg.ref_type != Ref_Const) {
            ss << "+" << arg.offset;
        }
        if (arg.is_indirect) {
            ss << "]";
        }
        if (arg.post_offset) {
            ss << "+" << arg.post_offset;
        }
        if (arg.type) {
            ss << ":" << type_name(&tmp_arena, arg.type);
        }
    };

    for (auto const &instr : prog.instrs) {
        ss << std::setw(c_padding) << (&instr - prog.instrs.data) * sizeof(Instr);

        if (instr.arg[0].ref_type != Ref_None) {
            _inspectArg(instr.arg[0]);
            ss << " := ";
        }

        ss << s_ir_names[instr.code];

        for (size_t i = 1; i < 3; i++) {
            if (instr.arg[i].ref_type != Ref_None) {
                ss << ((i > 1) ? ", " : " ");
                _inspectArg(instr.arg[i]);
            }
        }

        ss << "\n";
    }
}

} // namespace

void Program::deinit() {
    delete[] globals;
    funct_info.deinit();
    instrs.deinit();
    rodata.deinit();
}

string Program::inspect(Allocator *allocator) {
    std::ostringstream ss;
    _inspect(*this, ss);
    auto str = ss.str();

    char *data = (char *)allocator->alloc(str.size());
    std::memcpy(data, str.data(), str.size());

    return string{data, str.size()};
}

type_t Translator::translateFromIr(Program &prog, ir::Program const &ir) {
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

    prog.globals_t = type_get_tuple(&tmp_arena, {ir.globals.data, ir.globals.size});

    for (size_t fi = 0; auto const &funct : ir.functs) {
        assert(funct.next_block_id == funct.next_block_id && "ill-formed ir");

        auto &funct_info = prog.funct_info[funct.id] = {};
        funct_info.prog = &prog;
        funct_info.frame_t = type_get_tuple(&tmp_arena, {funct.locals.data, funct.locals.size});
        funct_info.first_instr = prog.instrs.size;

        funct_info.funct_t =
            type_get_fn(funct.ret_t, funct.args_t, fi++, interp_invoke, &funct_info);

        auto _pushConst = [&](value_t val) -> size_t {
            // TODO Unify aligned push to array
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
                switch (ref.ref_type) {
                case Ref_Frame:
                    arg.offset = type_tuple_offset(funct_info.frame_t, ref.value.index);
                    break;
                case Ref_Arg:
                    arg.offset = type_tuple_offset(funct.args_t, ref.value.index);
                    break;
                case Ref_Ret:
                    break;
                case Ref_Global:
                    arg.offset = type_tuple_offset(prog.globals_t, ref.value.index);
                    break;
                case Ref_Const:
                    arg.offset = _pushConst({ref.value.data, ref.type});
                    break;
                default:
                    assert(!"unreachable");
                case Ref_None:
                    break;
                }
                arg.offset += ref.offset;
                arg.post_offset = ref.post_offset;
                arg.type = ref.type;
                arg.ref_type = ref.ref_type;
                arg.is_indirect = ref.is_indirect;
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
            }
        };

        for (auto const &block : ir.blocks.slice(funct.first_block, funct.block_count)) {
            auto &block_info = block_info_ar[block.id] = {};
            block_info.first_instr = prog.instrs.size;

            for (auto const &ir_instr : ir.instrs.slice(block.first_instr, block.instr_count)) {
                auto &instr = prog.instrs.push() = {};

                instr.code = ir_instr.code;

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

    size_t entry_point_id = ir.entry_point_id == -1ul ? 0 : ir.entry_point_id;
    assert(entry_point_id < prog.funct_info.size && "ill-formed ir");
    return prog.funct_info[entry_point_id].funct_t;
}

} // namespace vm
} // namespace nk
