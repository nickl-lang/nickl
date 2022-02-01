#include "nk/vm/interp.hpp"

#include <cassert>

#include "nk/common/logger.hpp"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::interp)

Program const *s_prog;

struct InterpContext {
    struct Base { // repeats order of ERefType values
        uint8_t *null;
        uint8_t *frame;
        uint8_t const *arg;
        uint8_t *ret;
        uint8_t *global;
        uint8_t const *rodata;
        uint8_t const *instr;
    };
    union {
        uint8_t *base_ar[Ref_Count];
        Base base;
    };
    ArenaAllocator stack;
    Instr const *pinstr;
    bool is_initialized;
};

struct InterpFrame {
    uint8_t *base_frame;
    uint8_t const *base_arg;
    uint8_t *base_ret;
    Instr const *pinstr;
};

thread_local InterpContext ctx;
Array<uint8_t> s_globals;

void _initCtx() {
    ctx.stack.init();

    ctx.base.global = s_globals.data;
    ctx.base.rodata = s_prog->rodata.data;
    ctx.base.instr = (uint8_t *)s_prog->instrs.data;

    ctx.is_initialized = true;
}

void _deinitCtx() {
    ctx.stack.deinit();

    ctx.is_initialized = false;
}

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

INTERP(nop){LOG_DBG(__FUNCTION__)}

INTERP(enter) {
    LOG_DBG(__FUNCTION__)
    INTERP_NOT_IMPLEMENTED(enter);
}

INTERP(leave) {
    LOG_DBG(__FUNCTION__)
    INTERP_NOT_IMPLEMENTED(leave);
}

INTERP(ret) {
}

void _jumpTo(Ref ref) {
    // NOTE - 1 is because pinstr gets incremented after each instruction
    ctx.pinstr = &_getRef<Instr>(ref) - 1;
}

INTERP(jmp) {
    LOG_DBG(__FUNCTION__)

    _jumpTo(ctx.pinstr->arg[1]);
}

INTERP(jmpz) {
    LOG_DBG(__FUNCTION__)

    auto cond = _getDynRef(ctx.pinstr->arg[1]);

    assert(cond.type->typeclass_id == Type_Numeric);

    val_numeric_visit(cond, [=](auto val) {
        if (!val) {
            _jumpTo(ctx.pinstr->arg[2]);
        }
    });
}

INTERP(jmpnz) {
    LOG_DBG(__FUNCTION__)

    auto cond = _getDynRef(ctx.pinstr->arg[1]);

    assert(cond.type->typeclass_id == Type_Numeric);

    val_numeric_visit(cond, [=](auto val) {
        if (val) {
            _jumpTo(ctx.pinstr->arg[2]);
        }
    });
}

INTERP(cast) {
    LOG_DBG(__FUNCTION__)

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
    LOG_DBG(__FUNCTION__)

    auto ret = _getDynRef(ctx.pinstr->arg[0]);
    auto fn = _getDynRef(ctx.pinstr->arg[1]);
    auto args = _getDynRef(ctx.pinstr->arg[2]);

    val_fn_invoke(val_typeof(fn), ret, args);
}

INTERP(mov) {
    LOG_DBG(__FUNCTION__)

    auto dst = _getDynRef(ctx.pinstr->arg[0]);
    auto src = _getDynRef(ctx.pinstr->arg[1]);

    assert(val_sizeof(dst) == val_sizeof(src));

    std::memcpy(val_data(dst), val_data(src), val_sizeof(dst));
}

INTERP(lea) {
    LOG_DBG(__FUNCTION__)

    auto dst = _getDynRef(ctx.pinstr->arg[0]);
    auto src = _getDynRef(ctx.pinstr->arg[1]);

    assert(val_typeclassid(dst) == Type_Ptr);

    val_as(void *, dst) = val_data(src);
}

INTERP(neg) {
    LOG_DBG(__FUNCTION__)
    INTERP_NOT_IMPLEMENTED(neg);
}

INTERP(compl ) {
    LOG_DBG(__FUNCTION__)
    INTERP_NOT_IMPLEMENTED(compl );
}

INTERP(not ) {
    LOG_DBG(__FUNCTION__)
    INTERP_NOT_IMPLEMENTED(not );
}

template <class F>
void _numericBinOp(F &&op) {
    auto dst = _getDynRef(ctx.pinstr->arg[0]);
    auto lhs = _getDynRef(ctx.pinstr->arg[1]);
    auto rhs = _getDynRef(ctx.pinstr->arg[2]);

    // TODO implement errors for interp
    assert(val_typeid(dst) == val_typeid(lhs));
    assert(val_typeid(dst) == val_typeid(rhs));
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

    assert(val_typeid(dst) == val_typeid(lhs));
    assert(val_typeid(dst) == val_typeid(rhs));
    assert(dst.type->typeclass_id == Type_Numeric && dst.type->typeclass_id < Float32);

    val_numeric_visit_int(dst, [&](auto &dst_val) {
        using T = std::decay_t<decltype(dst_val)>;
        dst_val = op(val_as(T, lhs), val_as(T, rhs));
    });
}

INTERP(add) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs + rhs;
    });
}

INTERP(sub) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs - rhs;
    });
}

INTERP(mul) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs * rhs;
    });
}

INTERP(div) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs / rhs;
    });
}

INTERP(mod) {
    LOG_DBG(__FUNCTION__)
    _numericBinOpInt([](auto lhs, auto rhs) {
        return lhs % rhs;
    });
}

INTERP(bitand) {
    LOG_DBG(__FUNCTION__)
    _numericBinOpInt([](auto lhs, auto rhs) {
        return lhs & rhs;
    });
}

INTERP(bitor) {
    LOG_DBG(__FUNCTION__)
    _numericBinOpInt([](auto lhs, auto rhs) {
        return lhs | rhs;
    });
}

INTERP(xor) {
    LOG_DBG(__FUNCTION__)
    _numericBinOpInt([](auto lhs, auto rhs) {
        return lhs ^ rhs;
    });
}

INTERP(lsh) {
    LOG_DBG(__FUNCTION__)
    _numericBinOpInt([](auto lhs, auto rhs) {
        return lhs << rhs;
    });
}

INTERP(rsh) {
    LOG_DBG(__FUNCTION__)
    _numericBinOpInt([](auto lhs, auto rhs) {
        return lhs >> rhs;
    });
}

INTERP(and) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs && rhs;
    });
}

INTERP(or) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs || rhs;
    });
}

INTERP(eq) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs == rhs;
    });
}

INTERP(ge) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs >= rhs;
    });
}

INTERP(gt) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs > rhs;
    });
}

INTERP(le) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs <= rhs;
    });
}

INTERP(lt) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs < rhs;
    });
}

INTERP(ne) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp([](auto lhs, auto rhs) {
        return lhs != rhs;
    });
}

using InterpFunc = void (*)();

InterpFunc s_funcs[] = {
#define X(NAME) _interp_##NAME,
#include "nk/vm/ir.inl"
};

} // namespace

void interp_init(Program const *prog) {
    // TODO Maybe init prog ptr from the first function call in the thread
    s_prog = prog;

    // TODO Think if this check can be removed
    if (prog->globals_t->size > 0) {
        s_globals.push(prog->globals_t->size);
    }
}

void interp_deinit() {
    s_globals.deinit();

    s_prog = nullptr;
}

void interp_invoke(type_t self, value_t ret, value_t args) {
    // TODO Add logs everywhere
    // TODO Integrate profiling

    assert(s_prog && "program is not initialized");

    LOG_TRC(__FUNCTION__);

    bool was_uninitialized = !ctx.is_initialized;
    if (was_uninitialized) {
        _initCtx();
    }

    FunctInfo const &fn_info = *(FunctInfo *)self->as.fn.closure;

    // TODO Refactor hack with arena and _seq
    size_t const prev_stack_top = ctx.stack._seq.size;

    InterpFrame fr{
        .base_frame =
            (uint8_t *)ctx.stack.alloc_aligned(fn_info.frame_t->size, fn_info.frame_t->alignment),
        .base_arg = (uint8_t *)val_data(args),
        .base_ret = (uint8_t *)val_data(ret),
        .pinstr = &s_prog->instrs[fn_info.first_instr]};

    std::swap(ctx.base.frame, fr.base_frame);
    std::swap(ctx.base.arg, fr.base_arg);
    std::swap(ctx.base.ret, fr.base_ret);
    std::swap(ctx.pinstr, fr.pinstr);

    LOG_DBG("jumping to %lx", fn_info.first_instr * sizeof(Instr));

    while (ctx.pinstr->code != ir::ir_ret) {
        assert(ctx.pinstr->code < ir::Ir_Count && "unknown instruction");
        s_funcs[ctx.pinstr->code]();
        ctx.pinstr++;
    }

    ctx.base.frame = fr.base_frame;
    ctx.base.arg = fr.base_arg;
    ctx.base.ret = fr.base_ret;
    ctx.pinstr = fr.pinstr;

    ctx.stack._seq.pop(ctx.stack._seq.size - prev_stack_top);

    if (was_uninitialized) {
        _deinitCtx();
    }
}

} // namespace vm
} // namespace nk
