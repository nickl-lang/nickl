#include "nk/vm/vm.hpp"

#include <cassert>

#include "nk/common/logger.hpp"

namespace nk {
namespace vm {

void Translator::init() {
    prog = {};
}

void Translator::deinit() {
    prog.instrs.deinit();
    prog.rodata.deinit();
}

void Translator::translateFromIr(ir::Program const &ir) {
    prog.instrs.init(ir.instrs.size);

    enum ERelocType {
        Reloc_Funct,
        Reloc_Block,
    };

    struct Reloc {
        size_t instr_index;
        size_t arg;
        size_t target_index;
        ERelocType reloc_type;
    };

    struct BlockInfo {
        size_t first_instr;
    };

    struct FunctInfo {
        type_t ret_t;
        type_t args_t;
        type_t frame_t;
        size_t first_instr;
        type_t funct_t;
    };

    assert(ir.next_funct_id == ir.functs.size && "ill-formed ir");

    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); });

    auto funct_info_ar = tmp_arena.alloc<FunctInfo>(ir.functs.size);
    auto block_info_ar = tmp_arena.alloc<BlockInfo>(ir.blocks.size);

    Array<Reloc> relocs{};
    DEFER({ relocs.deinit(); });

    prog.globals_t = type_get_tuple(&tmp_arena, {ir.globals.data, ir.globals.size});

    for (size_t fi = 0; fi < ir.functs.size; fi++) {
        auto &funct = ir.functs.data[fi];

        assert(funct.next_block_id == funct.next_block_id && "ill-formed ir");

        auto &funct_info = funct_info_ar[funct.id] = {};
        funct_info.ret_t = funct.ret_t;
        funct_info.args_t = funct.args_t;
        funct_info.frame_t = type_get_tuple(&tmp_arena, {funct.locals.data, funct.locals.size});
        funct_info.first_instr = prog.instrs.size;

        auto _pushConst = [&](value_t val) -> size_t {
            uint8_t *ptr = &prog.rodata.push(val_sizeof(val) + val_alignof(val) - 1);
            ptr = (uint8_t *)roundUpSafe((size_t)ptr, val_alignof(val));
            return ptr - prog.rodata.data;
        };

        auto _compileArg = [&](size_t ii, size_t ai, Ref &arg, ir::Arg const &ir_arg) {
            switch (ir_arg.arg_type) {
            case ir::Arg_None:
                arg.ref_type = ir::Ref_None;
                break;
            case ir::Arg_Ref:
                switch (ir_arg.as.ref.ref_type) {
                case ir::Ref_Frame:
                    arg.offset = type_tuple_offset(funct_info.frame_t, ir_arg.as.ref.value.index);
                    break;
                case ir::Ref_Arg:
                    arg.offset = type_tuple_offset(funct.args_t, ir_arg.as.ref.value.index);
                    break;
                case ir::Ref_Ret:
                    break;
                case ir::Ref_Global:
                    arg.offset = type_tuple_offset(prog.globals_t, ir_arg.as.ref.value.index);
                    break;
                case ir::Ref_Const:
                    arg.offset = _pushConst({ir_arg.as.ref.value.data, ir_arg.as.ref.type});
                    break;
                default:
                    assert(!"unreachable");
                    break;
                }
                arg.post_offset = ir_arg.as.ref.offset;
                arg.type = ir_arg.as.ref.type;
                arg.ref_type = ir_arg.as.ref.ref_type;
                arg.is_indirect = ir_arg.as.ref.is_indirect;
                break;
            case ir::Arg_BlockId:
                arg.ref_type = ir::Ref_Instr;
                relocs.push() = {
                    .instr_index = ii,
                    .arg = ai,
                    .target_index = ir_arg.as.index,
                    .reloc_type = Reloc_Block,
                };
                break;
            case ir::Arg_FunctId:
                arg.ref_type = ir::Ref_Const;
                relocs.push() = {
                    .instr_index = ii,
                    .arg = ai,
                    .target_index = ir_arg.as.index,
                    .reloc_type = Reloc_Funct,
                };
                break;
            }
        };

        for (size_t bi = funct.first_block; bi < funct.first_block + funct.block_count; bi++) {
            auto &block = ir.blocks.data[bi];

            auto &block_info = block_info_ar[block.id] = {};
            block_info.first_instr = prog.instrs.size;

            for (size_t ii = block.first_instr; ii < block.first_instr + block.instr_count; ii++) {
                auto &ir_instr = ir.instrs.data[ii];
                auto &instr = prog.instrs.push() = {};

                instr.code = ir_instr.code;

                for (size_t ai = 0; ai < 3; ai++) {
                    _compileArg(prog.instrs.size - 1, ai, instr.arg[ai], ir_instr.arg[ai]);
                }
            }
        }
    }

    for (size_t i = 0; i < ir.functs.size; i++) {
        auto &info = funct_info_ar[i];

        // TODO fill body_ptr and closure
        info.funct_t = type_get_fn(info.ret_t, info.args_t, i, nullptr, nullptr);
    }

    for (size_t i = 0; i < relocs.size; i++) {
        auto const &reloc = relocs.data[i];

        Ref &ref = prog.instrs.data[reloc.instr_index].arg[reloc.arg];

        switch (reloc.reloc_type) {
        case Reloc_Funct:
            ref.type = funct_info_ar[reloc.target_index].funct_t;
            break;
        case Reloc_Block:
            ref.offset = block_info_ar[reloc.target_index].first_instr * sizeof(Instr);
            break;
        }
    }
}

string Translator::inspect(Allocator *allocator) {
    (void)allocator;
    return cstr_to_str("Translator::inspect is not implemented");
}

} // namespace vm
} // namespace nk
