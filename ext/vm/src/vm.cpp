#include "nk/vm/vm.hpp"

namespace nk {
namespace vm {

namespace {

size_t _calcTupleSize(TypeArray types) {
    size_t alignment = 0;
    size_t offset = 0;

    for (size_t i = 0; i < types.size; i++) {
        type_t const type = types.data[i];
        alignment = maxu(alignment, type->alignment);
        offset = roundUpSafe(offset, type->alignment);
        offset += type->size;
    }

    return roundUpSafe(offset, alignment);
}

} // namespace

void Translator::init() {
    prog.arena.init();
    prog.instrs.init();
}

void Translator::deinit() {
    prog.instrs.deinit();
    prog.arena.deinit();
}

void Translator::translateFromIr(ir::Program const &ir) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    prog.instrs.init(ir.instrs.size);

    struct Reloc {
        size_t index;
        size_t arg;
    };

    struct BlockInfo {
        size_t first_instr = 0;
        Array<Reloc> relocs{};
    };

    struct FunctInfo {
        size_t frame_size = 0;
        size_t first_instr = 0;
        Array<Reloc> relocs{};
    };

    auto funct_info_ar = Array<FunctInfo>::create(ir.functs.size);
    auto block_info_ar = Array<BlockInfo>::create(ir.blocks.size);
    auto _defer = [&]() {
        for (size_t i = 0; i < block_info_ar.size; i++) {
            block_info_ar.data[i].relocs.deinit();
        }
        block_info_ar.deinit();
        for (size_t i = 0; i < funct_info_ar.size; i++) {
            funct_info_ar.data[i].relocs.deinit();
        }
        funct_info_ar.deinit();
    };
    DEFER({ _defer(); });

    prog.globals_size = _calcTupleSize({ir.globals.data, ir.globals.size});

    auto _compileArg = [](Ref &arg, ir::Arg const &ir_arg) {
    };

    for (size_t fi = 0; fi < ir.functs.size; fi++) {
        auto &funct = ir.functs.data[fi];

        auto &funct_info = funct_info_ar.push() = {};
        funct_info.frame_size = _calcTupleSize({funct.locals.data, funct.locals.size});

        for (size_t bi = funct.first_block; bi < funct.first_block + funct.block_count; bi++) {
            auto &block = ir.blocks.data[bi];

            auto &block_info = block_info_ar.push() = {};
            block_info.first_instr = prog.instrs.size;

            for (size_t ii = block.first_instr; ii < block.first_instr + block.instr_count; ii++) {
                auto &ir_instr = ir.instrs.data[ii];
                auto &instr = prog.instrs.push() = {};

                instr.code = ir_instr.code;

                for (size_t j = 0; j < 3; j++) {
                    _compileArg(instr.arg[j], ir_instr.arg[j]);
                }
            }
        }
    }

    // Create function types with pointers to instructions
    // Fill refs to them
}

string Translator::inspect(Allocator *allocator) {
    (void)allocator;
    return cstr_to_str("Translator::inspect is not implemented");
}

} // namespace vm
} // namespace nk
