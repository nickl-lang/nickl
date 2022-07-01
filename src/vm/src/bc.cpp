#include "nk/vm/bc.hpp"

#include <cassert>

#include "find_library.hpp"
#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nk/common/string_builder.hpp"
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

LOG_USE_SCOPE(nk::vm);

void _inspect(Program const &prog, StringBuilder &sb) {
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

    for (auto const &instr : prog.instrs) {
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

string Program::inspect(Allocator &allocator) const {
    StringBuilder sb{};
    _inspect(*this, sb);
    return sb.moveStr(allocator);
}

} // namespace bc
} // namespace vm
} // namespace nk
