#include "nk/vm/interp.hpp"

#include <cassert>

#include "nk/common/logger.hpp"
#include "nk/common/profiler.hpp"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::interp)

struct ProgramFrame {
    uint8_t *base_global;
    uint8_t *base_rodata;
    uint8_t *base_instr;
};

struct ControlFrame {
    size_t stack_frame_base;
    uint8_t *base_frame;
    uint8_t *base_arg;
    uint8_t *base_ret;
    Instr const *pinstr;
};

struct InterpContext {
    struct Base { // repeats order of ERefType values
        uint8_t *null;
        uint8_t *frame;
        uint8_t *arg;
        uint8_t *ret;
        uint8_t *global;
        uint8_t *rodata;
        uint8_t *instr;
    };
    union {
        uint8_t *base_ar[Ref_Count];
        Base base;
    };
    ArenaAllocator stack;
    Array<ControlFrame> ctrl_stack;
    size_t stack_frame_base;
    Instr const *pinstr;
    bool is_initialized;
    bool returned;
};

thread_local InterpContext ctx;

template <class T>
T &_getRef(Ref ref) {
    auto ptr = ctx.base_ar[ref.ref_type] + ref.offset;
    if (ref.is_indirect) {
        ptr = *reinterpret_cast<uint8_t **>(ptr);
    }
    return *reinterpret_cast<T *>(ptr + ref.post_offset);
}

value_t _getDynRef(Ref ref) {
    return {&_getRef<uint8_t>(ref), ref.type};
}

#define INTERP(NAME) void _interp_##NAME()
#define INTERP_NOT_IMPLEMENTED(NAME) assert(!"instr not implemented" #NAME)

INTERP(nop){EASY_FUNCTION(profiler::colors::Blue200)}

INTERP(enter) {
    EASY_FUNCTION(profiler::colors::Blue200)
    INTERP_NOT_IMPLEMENTED(enter);
}

INTERP(leave) {
    EASY_FUNCTION(profiler::colors::Blue200)
    INTERP_NOT_IMPLEMENTED(leave);
}

void _jumpTo(Instr const *pinstr) {
    // NOTE - 1 is because pinstr gets incremented after each instruction
    ctx.pinstr = pinstr - 1;
}

void _jumpTo(Ref ref) {
    _jumpTo(&_getRef<Instr>(ref));
}

INTERP(ret) {
    EASY_FUNCTION(profiler::colors::Blue200)

    const auto &fr = ctx.ctrl_stack.pop();

    ctx.stack._seq.pop(ctx.stack._seq.size - ctx.stack_frame_base);

    ctx.stack_frame_base = fr.stack_frame_base;
    ctx.base.frame = fr.base_frame;
    ctx.base.arg = fr.base_arg;
    ctx.base.ret = fr.base_ret;

    ctx.returned = true;

    _jumpTo(fr.pinstr);
}

INTERP(jmp) {
    EASY_FUNCTION(profiler::colors::Blue200)

    _jumpTo(ctx.pinstr->arg[1]);
}

INTERP(jmpz) {
    EASY_FUNCTION(profiler::colors::Blue200)

    auto cond = _getDynRef(ctx.pinstr->arg[1]);

    assert(cond.type->typeclass_id == Type_Numeric);

    val_numeric_visit(cond, [=](auto val) {
        if (!val) {
            _jumpTo(ctx.pinstr->arg[2]);
        }
    });
}

INTERP(jmpnz) {
    EASY_FUNCTION(profiler::colors::Blue200)

    auto cond = _getDynRef(ctx.pinstr->arg[1]);

    assert(cond.type->typeclass_id == Type_Numeric);

    val_numeric_visit(cond, [=](auto val) {
        if (val) {
            _jumpTo(ctx.pinstr->arg[2]);
        }
    });
}

INTERP(cast) {
    EASY_FUNCTION(profiler::colors::Blue200)

    auto dst = _getDynRef(ctx.pinstr->arg[0]);
    auto type = _getDynRef(ctx.pinstr->arg[1]);
    auto arg = _getDynRef(ctx.pinstr->arg[2]);

    assert(dst.type->typeclass_id == Type_Numeric);
    assert(type.type->typeclass_id == Type_Typeref);
    assert(val_typeid(dst) == val_as(type_t, type)->id);
    assert(arg.type->typeclass_id == Type_Numeric);

    val_numeric_visit(dst, [&](auto &dst_val) {
        using T = std::decay_t<decltype(dst_val)>;
        val_numeric_visit(arg, [&](auto arg_val) {
            dst_val = static_cast<T>(arg_val);
        });
    });
}

INTERP(call) {
    EASY_FUNCTION(profiler::colors::Blue200)

    auto ret = _getDynRef(ctx.pinstr->arg[0]);
    auto fn = _getDynRef(ctx.pinstr->arg[1]);
    auto args = _getDynRef(ctx.pinstr->arg[2]);

    val_fn_invoke(val_typeof(fn), ret, args);
}

INTERP(mov) {
    EASY_FUNCTION(profiler::colors::Blue200)

    auto dst = _getDynRef(ctx.pinstr->arg[0]);
    auto src = _getDynRef(ctx.pinstr->arg[1]);

    assert(val_sizeof(dst) == val_sizeof(src));

    std::memcpy(val_data(dst), val_data(src), val_sizeof(dst));
}

INTERP(lea) {
    EASY_FUNCTION(profiler::colors::Blue200)

    auto dst = _getDynRef(ctx.pinstr->arg[0]);
    auto src = _getDynRef(ctx.pinstr->arg[1]);

    assert(val_typeclassid(dst) == Type_Ptr);

    val_as(void *, dst) = val_data(src);
}

INTERP(neg) {
    EASY_FUNCTION(profiler::colors::Blue200)

    INTERP_NOT_IMPLEMENTED(neg);
}

INTERP(compl ) {
    EASY_FUNCTION(profiler::colors::Blue200)

    INTERP_NOT_IMPLEMENTED(compl );
}

INTERP(not ) {
    EASY_FUNCTION(profiler::colors::Blue200)

    INTERP_NOT_IMPLEMENTED(not );
}

template <class F>
void _numericBinOp(F &&op) {
    auto dst = _getDynRef(ctx.pinstr->arg[0]);
    auto lhs = _getDynRef(ctx.pinstr->arg[1]);
    auto rhs = _getDynRef(ctx.pinstr->arg[2]);

    assert(val_typeid(dst) == val_typeid(lhs) && val_typeid(dst) == val_typeid(rhs));
    assert(dst.type->typeclass_id == Type_Numeric);

    val_numeric_visit(dst, [&](auto &dst_val) {
        using T = std::decay_t<decltype(dst_val)>;
        dst_val = op(val_as(T, lhs), val_as(T, rhs));
    });
}

template <class F>
void _numericBinOpInt(F &&op) {
    auto dst = _getDynRef(ctx.pinstr->arg[0]);
    auto lhs = _getDynRef(ctx.pinstr->arg[1]);
    auto rhs = _getDynRef(ctx.pinstr->arg[2]);

    assert(val_typeid(dst) == val_typeid(lhs) && val_typeid(dst) == val_typeid(rhs));
    assert(dst.type->typeclass_id == Type_Numeric && dst.type->typeclass_id < Float32);

    val_numeric_visit_int(dst, [&](auto &dst_val) {
        using T = std::decay_t<decltype(dst_val)>;
        dst_val = op(val_as(T, lhs), val_as(T, rhs));
    });
}

INTERP(add) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs + rhs;
    });
}

INTERP(sub) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs - rhs;
    });
}

INTERP(mul) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs * rhs;
    });
}

INTERP(div) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs / rhs;
    });
}

INTERP(mod) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOpInt([](auto lhs, auto rhs) {
        return lhs % rhs;
    });
}

INTERP(bitand) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOpInt([](auto lhs, auto rhs) {
        return lhs & rhs;
    });
}

INTERP(bitor) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOpInt([](auto lhs, auto rhs) {
        return lhs | rhs;
    });
}

INTERP(xor) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOpInt([](auto lhs, auto rhs) {
        return lhs ^ rhs;
    });
}

INTERP(lsh) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOpInt([](auto lhs, auto rhs) {
        return lhs << rhs;
    });
}

INTERP(rsh) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOpInt([](auto lhs, auto rhs) {
        return lhs >> rhs;
    });
}

INTERP(and) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs && rhs;
    });
}

INTERP(or) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs || rhs;
    });
}

INTERP(eq) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOp([](auto lhs, auto rhs) {
        //@Refactor/Feature Make boolean instrs write to a special register
        return lhs == rhs;
    });
}

INTERP(ge) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs >= rhs;
    });
}

INTERP(gt) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs > rhs;
    });
}

INTERP(le) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs <= rhs;
    });
}

INTERP(lt) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs < rhs;
    });
}

INTERP(ne) {
    EASY_FUNCTION(profiler::colors::Blue200)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs != rhs;
    });
}

using InterpFunc = void (*)();

InterpFunc s_funcs[] = {
#define X(NAME) _interp_##NAME,
#include "nk/vm/ir.inl"
};

void _setupFrame(Instr *pinstr, type_t frame_t, value_t ret, value_t args) {
    ctx.ctrl_stack.push() = {
        .stack_frame_base = ctx.stack_frame_base,
        .base_frame = ctx.base.frame,
        .base_arg = ctx.base.arg,
        .base_ret = ctx.base.ret,
        .pinstr = ctx.pinstr,
    };

    //@Refactor Implement stack frame push/pop with Sequence
    ctx.stack_frame_base = ctx.stack._seq.size;
    ctx.base.frame = (uint8_t *)ctx.stack.alloc_aligned(frame_t->size, frame_t->alignment);
    ctx.base.arg = (uint8_t *)val_data(args);
    ctx.base.ret = (uint8_t *)val_data(ret);
    ctx.pinstr = pinstr;

    LOG_DBG("stack_frame_base=%lu", ctx.stack_frame_base)
    LOG_DBG("frame=%p", ctx.base.frame)
    LOG_DBG("arg=%p", ctx.base.arg)
    LOG_DBG("ret=%p", ctx.base.ret)
    LOG_DBG("pinstr=%p", ctx.pinstr)
}

} // namespace

void interp_invoke(type_t self, value_t ret, value_t args) {
    EASY_FUNCTION(profiler::colors::Blue200)

    LOG_TRC(__FUNCTION__);

    EASY_NONSCOPED_BLOCK("setup", profiler::colors::Blue200)

    FunctInfo const &fn = *(FunctInfo *)self->as.fn.closure;
    Program const &prog = *fn.prog;

    LOG_DBG("program @%p", &prog)

    bool was_uninitialized = !ctx.is_initialized;
    if (was_uninitialized) {
        LOG_TRC("initializing stack...")
        //@Feature Maybe implement the push to zero initialized Sequence
        ctx.stack.init();
        ctx.is_initialized = true;
    }

    ProgramFrame pfr{
        .base_global = ctx.base.global,
        .base_rodata = ctx.base.rodata,
        .base_instr = ctx.base.instr,
    };

    ctx.base.global = prog.globals.data;
    ctx.base.rodata = prog.rodata.data;
    ctx.base.instr = (uint8_t *)prog.instrs.data;

    LOG_DBG("global=%p", ctx.base.global)
    LOG_DBG("rodata=%p", ctx.base.rodata)
    LOG_DBG("instr=%p", ctx.base.instr)

    _setupFrame(&prog.instrs[fn.first_instr], fn.frame_t, ret, args);

    EASY_END_BLOCK

    EASY_NONSCOPED_BLOCK("exec", profiler::colors::Blue200)

    LOG_DBG("jumping to %lx", fn.first_instr * sizeof(Instr));

    while (!ctx.returned) {
        assert(ctx.pinstr->code < ir::Ir_Count && "unknown instruction");
        LOG_DBG(
            "instr: %lx %s",
            (ctx.pinstr - prog.instrs.data) * sizeof(Instr),
            s_op_names[ctx.pinstr->code])
        s_funcs[ctx.pinstr->code]();
        ctx.pinstr++;
    }

    ctx.returned = false;

    EASY_END_BLOCK

    EASY_NONSCOPED_BLOCK("teardown", profiler::colors::Blue200)

    LOG_TRC("exiting...");

    ctx.base.global = pfr.base_global;
    ctx.base.rodata = pfr.base_rodata;
    ctx.base.instr = pfr.base_instr;

    if (was_uninitialized) {
        LOG_TRC("deinitializing stack...")

        assert(ctx.stack._seq.size == 0 && "nonempty stack at exit");

        ctx.stack.deinit();
        ctx.ctrl_stack.deinit();

        ctx.is_initialized = false;
    }

    EASY_END_BLOCK
}

} // namespace vm
} // namespace nk
