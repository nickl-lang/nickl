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
        uint8_t *frame;
        uint8_t const *arg;
        uint8_t *ret;
        uint8_t *global;
        uint8_t const *rodata;
        uint8_t const *instr;
    };
    union {
        uint8_t *base_ar[Ref_End - Ref_Base];
        Base base;
    };
    ArenaAllocator stack;
    Instr const *next_instr;
    bool is_initialized;
};

thread_local InterpContext s_interp_ctx;
Array<uint8_t> s_globals;

void _initCtx() {
    auto &ctx = s_interp_ctx;

    ctx.stack.init();

    ctx.base.global = s_globals.data;
    ctx.base.rodata = s_prog->rodata.data;
    ctx.base.instr = (uint8_t *)s_prog->instrs.data;

    ctx.is_initialized = true;
}

void _deinitCtx() {
    auto &ctx = s_interp_ctx;

    ctx.stack.deinit();

    ctx.is_initialized = false;
}

template <class T>
T &_getRef(Ref const &arg) {
    auto ptr = s_interp_ctx.base_ar[arg.ref_type - Ref_Base] + arg.offset;
    if (arg.is_indirect) {
        ptr = *reinterpret_cast<uint8_t **>(ptr);
    }
    return *reinterpret_cast<T *>(ptr + arg.post_offset);
}

value_t _getDynRef(Ref const &arg) {
    return {&_getRef<uint8_t>(arg), arg.type};
}

#define INTERP(NAME) void _interp_##NAME(Instr const &instr)
#define INTERP_NOT_IMPLEMENTED(NAME) \
    (void)instr;                     \
    assert(!"instr not implemented" #NAME)

INTERP(nop) {
    LOG_DBG(__FUNCTION__)
    (void)instr;
}

INTERP(enter) {
    LOG_DBG(__FUNCTION__)
    INTERP_NOT_IMPLEMENTED(enter);
}

INTERP(leave) {
    LOG_DBG(__FUNCTION__)
    INTERP_NOT_IMPLEMENTED(leave);
}

INTERP(ret) {
    (void)instr;
}

INTERP(jmp) {
    LOG_DBG(__FUNCTION__)

    auto &target = _getRef<Instr>(instr.arg[1]);
    s_interp_ctx.next_instr = &target;
}

INTERP(jmpz) {
    LOG_DBG(__FUNCTION__)

    auto cond = _getDynRef(instr.arg[1]);

    assert(cond.type->typeclass_id == Type_Numeric);

    val_numeric_visit(cond, [=](auto val) {
        if (!val) {
            auto &target = _getRef<Instr>(instr.arg[2]);
            s_interp_ctx.next_instr = &target;
        }
    });
}

INTERP(jmpnz) {
    LOG_DBG(__FUNCTION__)

    auto cond = _getDynRef(instr.arg[1]);

    assert(cond.type->typeclass_id == Type_Numeric);

    val_numeric_visit(cond, [=](auto val) {
        if (val) {
            auto &target = _getRef<Instr>(instr.arg[2]);
            s_interp_ctx.next_instr = &target;
        }
    });
}

INTERP(cast) {
    LOG_DBG(__FUNCTION__)
    auto dst = _getDynRef(instr.arg[0]);
    auto type = _getDynRef(instr.arg[1]);
    auto arg = _getDynRef(instr.arg[2]);

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
    auto ret = _getDynRef(instr.arg[0]);
    auto fn = _getDynRef(instr.arg[1]);
    auto args = _getDynRef(instr.arg[2]);

    val_fn_invoke(val_typeof(fn), ret, args);
}

INTERP(mov) {
    LOG_DBG(__FUNCTION__)

    auto dst = _getDynRef(instr.arg[0]);
    auto src = _getDynRef(instr.arg[1]);

    assert(val_typeof(dst)->size == val_typeof(src)->size);

    std::memcpy(val_data(dst), val_data(src), val_typeof(dst)->size);
}

INTERP(lea) {
    LOG_DBG(__FUNCTION__)
    INTERP_NOT_IMPLEMENTED(lea);
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
void _numericBinOp(Instr const &instr, F &&op) {
    auto dst = _getDynRef(instr.arg[0]);
    auto lhs = _getDynRef(instr.arg[1]);
    auto rhs = _getDynRef(instr.arg[2]);

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
void _numericBinOpInt(Instr const &instr, F &&op) {
    auto dst = _getDynRef(instr.arg[0]);
    auto lhs = _getDynRef(instr.arg[1]);
    auto rhs = _getDynRef(instr.arg[2]);

    // TODO implement errors for interp
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
    _numericBinOp(instr, [](auto lhs, auto rhs) {
        return lhs + rhs;
    });
}

INTERP(sub) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp(instr, [](auto lhs, auto rhs) {
        return lhs - rhs;
    });
}

INTERP(mul) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp(instr, [](auto lhs, auto rhs) {
        return lhs * rhs;
    });
}

INTERP(div) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp(instr, [](auto lhs, auto rhs) {
        return lhs / rhs;
    });
}

INTERP(mod) {
    LOG_DBG(__FUNCTION__)
    _numericBinOpInt(instr, [](auto lhs, auto rhs) {
        return lhs % rhs;
    });
}

INTERP(bitand) {
    LOG_DBG(__FUNCTION__)
    _numericBinOpInt(instr, [](auto lhs, auto rhs) {
        return lhs & rhs;
    });
}

INTERP(bitor) {
    LOG_DBG(__FUNCTION__)
    _numericBinOpInt(instr, [](auto lhs, auto rhs) {
        return lhs | rhs;
    });
}

INTERP(xor) {
    LOG_DBG(__FUNCTION__)
    _numericBinOpInt(instr, [](auto lhs, auto rhs) {
        return lhs ^ rhs;
    });
}

INTERP(lsh) {
    LOG_DBG(__FUNCTION__)
    _numericBinOpInt(instr, [](auto lhs, auto rhs) {
        return lhs << rhs;
    });
}

INTERP(rsh) {
    LOG_DBG(__FUNCTION__)
    _numericBinOpInt(instr, [](auto lhs, auto rhs) {
        return lhs >> rhs;
    });
}

INTERP(and) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp(instr, [](auto lhs, auto rhs) {
        return lhs && rhs;
    });
}

INTERP(or) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp(instr, [](auto lhs, auto rhs) {
        return lhs || rhs;
    });
}

INTERP(eq) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp(instr, [](auto lhs, auto rhs) {
        return lhs == rhs;
    });
}

INTERP(ge) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp(instr, [](auto lhs, auto rhs) {
        return lhs >= rhs;
    });
}

INTERP(gt) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp(instr, [](auto lhs, auto rhs) {
        return lhs > rhs;
    });
}

INTERP(le) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp(instr, [](auto lhs, auto rhs) {
        return lhs <= rhs;
    });
}

INTERP(lt) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp(instr, [](auto lhs, auto rhs) {
        return lhs < rhs;
    });
}

INTERP(ne) {
    LOG_DBG(__FUNCTION__)
    _numericBinOp(instr, [](auto lhs, auto rhs) {
        return lhs != rhs;
    });
}

using InterpFunc = void (*)(Instr const &);

InterpFunc s_funcs[] = {
#define X(NAME) _interp_##NAME,
#include "nk/vm/ir.inl"
};

void _jumpTo(size_t instr_index) {
    LOG_DBG("jump to %lu", instr_index);
    auto &ctx = s_interp_ctx;
    ctx.next_instr = &s_prog->instrs.data[instr_index];
    while (ctx.next_instr->code != ir::ir_ret) {
        auto const &instr = *ctx.next_instr++;
        assert(instr.code < ir::Ir_Count && "unknown instruction");
        auto func = s_funcs[instr.code];
        func(instr);
    }
}

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

    auto &ctx = s_interp_ctx;

    bool was_uninitialized = !ctx.is_initialized;
    if (was_uninitialized) {
        _initCtx();
    }
    DEFER({
        if (was_uninitialized) {
            _deinitCtx();
        }
    });

    FunctInfo const &fn_info = *(FunctInfo *)self->as.fn.closure;

    // TODO Use stack on heap for the interp frame storage
    uint8_t *base_frame = nullptr;
    uint8_t const *base_arg = (uint8_t *)val_data(args);
    uint8_t *base_ret = (uint8_t *)val_data(ret);
    Instr const *next_instr = 0;

    size_t const prev_stack_top = ctx.stack._seq.size;
    if (fn_info.frame_t->size > 0) {
        // TODO Refactor hack with arena and _seq
        auto frame = ctx.stack.alloc_aligned(fn_info.frame_t->size, fn_info.frame_t->alignment);
        base_frame = (uint8_t *)frame;
    }
    DEFER({ ctx.stack._seq.pop(ctx.stack._seq.size - prev_stack_top); })

    std::swap(ctx.base.frame, base_frame);
    std::swap(ctx.base.arg, base_arg);
    std::swap(ctx.base.ret, base_ret);
    std::swap(ctx.next_instr, next_instr);

    DEFER({
        ctx.base.frame = base_frame;
        ctx.base.arg = base_arg;
        ctx.base.ret = base_ret;
        ctx.next_instr = next_instr;
    });

    _jumpTo(fn_info.first_instr);
}

} // namespace vm
} // namespace nk
