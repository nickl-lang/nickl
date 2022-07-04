#include "nk/vm/interp.hpp"

#include <cassert>

#include "nk/mem/stack_allocator.hpp"
#include "nk/mem/static_allocator.hpp"
#include "nk/str/static_string_builder.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
#include "nk/utils/utils.hpp"

namespace nk {
namespace vm {

namespace {

using namespace bc;

LOG_USE_SCOPE(nk::vm::interp);

struct ProgramFrame {
    uint8_t *base_global;
    uint8_t *base_rodata;
    uint8_t *base_instr;
    uint8_t *base_reg;
    Instr const *pinstr;
};

struct ControlFrame {
    StackAllocator::Frame stack_frame;
    uint8_t *base_frame;
    uint8_t *base_arg;
    uint8_t *base_ret;
    Instr const *pinstr;
};

struct InterpContext {
    struct Registers { // repeats the layout of ERegister
        uint64_t a;
        uint64_t b;
        uint64_t c;
        uint64_t d;
        uint64_t e;
        uint64_t f;
    };

    struct Base { // repeats the layout of bc::ERefType
        uint8_t *null;
        uint8_t *frame;
        uint8_t *arg;
        uint8_t *ret;
        uint8_t *global;
        uint8_t *rodata;
        uint8_t *reg;
        uint8_t *instr;
        uint8_t *abs;
    };

    union {
        uint8_t *base_ar[Ref_Count];
        Base base;
    };
    StackAllocator stack;
    Array<ControlFrame> ctrl_stack;
    Array<StackAllocator::Frame> stack_frames;
    StackAllocator::Frame stack_frame;
    Instr const *pinstr;
    bool is_initialized;
    Registers reg;
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

void _jumpTo(Instr const *pinstr) {
    LOG_DBG("jumping to instr@%p", pinstr);
    ctx.pinstr = pinstr;
}

void _jumpTo(Ref ref) {
    _jumpTo(&_getRef<Instr>(ref));
}

void _jumpCall(type_t frame_t, value_t ret, value_t args, Instr const *pinstr) {
    *ctx.ctrl_stack.push() = {
        .stack_frame = ctx.stack_frame,
        .base_frame = ctx.base.frame,
        .base_arg = ctx.base.arg,
        .base_ret = ctx.base.ret,
        .pinstr = ctx.pinstr,
    };

    ctx.stack_frame = ctx.stack.pushFrame();
    ctx.base.frame = (uint8_t *)ctx.stack.alloc_aligned(frame_t->size, frame_t->alignment);
    ctx.base.arg = (uint8_t *)val_data(args);
    ctx.base.ret = (uint8_t *)val_data(ret);

    _jumpTo(pinstr);

    LOG_DBG("stack_frame=%lu", ctx.stack_frame.size);
    LOG_DBG("frame=%p", ctx.base.frame);
    LOG_DBG("arg=%p", ctx.base.arg);
    LOG_DBG("ret=%p", ctx.base.ret);
    LOG_DBG("pinstr=%p", ctx.pinstr);
}

#define INTERP(NAME) void _interp_##NAME(Instr const &instr)

INTERP(nop) {
    (void)instr;
}

INTERP(ret) {
    (void)instr;

    const auto &fr = ctx.ctrl_stack.back();
    ctx.ctrl_stack.pop();

    ctx.stack.popFrame(ctx.stack_frame);

    ctx.stack_frame = fr.stack_frame;
    ctx.base.frame = fr.base_frame;
    ctx.base.arg = fr.base_arg;
    ctx.base.ret = fr.base_ret;

    _jumpTo(fr.pinstr);
}

INTERP(enter) {
    (void)instr;

    *ctx.stack_frames.push() = ctx.stack.pushFrame();
}

INTERP(leave) {
    (void)instr;

    ctx.stack.popFrame(ctx.stack_frames.back());
    ctx.stack_frames.pop();
}

INTERP(jmp) {
    _jumpTo(instr.arg[1]);
}

INTERP(jmpz) {
    auto cond = _getDynRef(instr.arg[1]);

    assert(val_typeclassid(cond) == Type_Numeric);

    val_numeric_visit(cond, [=](auto val) {
        if (!val) {
            _jumpTo(instr.arg[2]);
        }
    });
}

INTERP(jmpnz) {
    auto cond = _getDynRef(instr.arg[1]);

    assert(val_typeclassid(cond) == Type_Numeric);

    val_numeric_visit(cond, [=](auto val) {
        if (val) {
            _jumpTo(instr.arg[2]);
        }
    });
}

INTERP(cast) {
    auto dst = _getDynRef(instr.arg[0]);
    auto type = _getDynRef(instr.arg[1]);
    auto arg = _getDynRef(instr.arg[2]);

    assert(val_typeclassid(dst) == Type_Numeric);
    assert(val_typeclassid(type) == Type_Typeref);
    assert(val_typeid(dst) == val_as(type_t, type)->id);
    assert(val_typeclassid(arg) == Type_Numeric);

    val_numeric_visit(dst, [&](auto &dst_val) {
        using T = std::decay_t<decltype(dst_val)>;
        val_numeric_visit(arg, [&](auto arg_val) {
            dst_val = static_cast<T>(arg_val);
        });
    });
}

INTERP(call) {
    auto ret = _getDynRef(instr.arg[0]);
    auto fn = _getDynRef(instr.arg[1]);
    auto args = _getDynRef(instr.arg[2]);

    val_fn_invoke(val_typeof(fn), ret, args);
}

INTERP(call_jmp) {
    auto ret = _getDynRef(instr.arg[0]);
    auto fn = _getDynRef(instr.arg[1]);
    auto args = _getDynRef(instr.arg[2]);

    FunctInfo const &info = *(FunctInfo *)val_typeof(fn)->as.fn.closure;

    _jumpCall(info.frame_t, ret, args, &info.prog->instrs[info.first_instr]);
}

INTERP(mov) {
    auto dst = _getDynRef(instr.arg[0]);
    auto src = _getDynRef(instr.arg[1]);

    assert(val_sizeof(dst) == val_sizeof(src));

    std::memcpy(val_data(dst), val_data(src), val_sizeof(dst));
}

INTERP(mov_8) {
    _getRef<uint8_t>(instr.arg[0]) = _getRef<uint8_t>(instr.arg[1]);
}

INTERP(mov_16) {
    _getRef<uint16_t>(instr.arg[0]) = _getRef<uint16_t>(instr.arg[1]);
}

INTERP(mov_32) {
    _getRef<uint32_t>(instr.arg[0]) = _getRef<uint32_t>(instr.arg[1]);
}

INTERP(mov_64) {
    _getRef<uint64_t>(instr.arg[0]) = _getRef<uint64_t>(instr.arg[1]);
}

INTERP(lea) {
    _getRef<void *>(instr.arg[0]) = &_getRef<uint8_t>(instr.arg[1]);
}

INTERP(neg) {
    auto dst = _getDynRef(instr.arg[0]);
    auto arg = _getDynRef(instr.arg[1]);

    assert(val_typeid(dst) == val_typeid(arg));
    assert(val_typeclassid(dst) == Type_Numeric);

    val_numeric_visit(dst, [&](auto &dst_val) {
        using T = std::decay_t<decltype(dst_val)>;
        dst_val = -val_as(T, arg);
    });
}

INTERP(compl ) {
    auto dst = _getDynRef(instr.arg[0]);
    auto arg = _getDynRef(instr.arg[1]);

    assert(val_typeid(dst) == val_typeid(arg));
    assert(val_typeclassid(dst) == Type_Numeric && val_typeof(dst)->as.num.value_type < Float32);

    val_numeric_visit_int(dst, [&](auto &dst_val) {
        using T = std::decay_t<decltype(dst_val)>;
        dst_val = ~val_as(T, arg);
    });
}

INTERP(not ) {
    auto dst = _getDynRef(instr.arg[0]);
    auto arg = _getDynRef(instr.arg[1]);

    assert(val_typeid(dst) == val_typeid(arg));
    assert(val_typeclassid(dst) == Type_Numeric);

    val_numeric_visit(dst, [&](auto &dst_val) {
        using T = std::decay_t<decltype(dst_val)>;
        dst_val = !val_as(T, arg);
    });
}

INTERP(eq) {
    auto dst = _getDynRef(instr.arg[0]);
    auto lhs = _getDynRef(instr.arg[1]);
    auto rhs = _getDynRef(instr.arg[2]);

    assert(val_sizeof(dst) == 1);
    assert(val_sizeof(lhs) == val_sizeof(rhs));

    _getRef<int8_t>(instr.arg[0]) = std::memcmp(val_data(dst), val_data(lhs), val_sizeof(rhs)) == 0;
}

INTERP(eq_8) {
    _getRef<int8_t>(instr.arg[0]) =
        _getRef<uint8_t>(instr.arg[1]) == _getRef<uint8_t>(instr.arg[2]);
}

INTERP(eq_16) {
    _getRef<int8_t>(instr.arg[0]) =
        _getRef<uint16_t>(instr.arg[1]) == _getRef<uint16_t>(instr.arg[2]);
}

INTERP(eq_32) {
    _getRef<int8_t>(instr.arg[0]) =
        _getRef<uint32_t>(instr.arg[1]) == _getRef<uint32_t>(instr.arg[2]);
}

INTERP(eq_64) {
    _getRef<int8_t>(instr.arg[0]) =
        _getRef<uint64_t>(instr.arg[1]) == _getRef<uint64_t>(instr.arg[2]);
}

INTERP(ne) {
    auto dst = _getDynRef(instr.arg[0]);
    auto lhs = _getDynRef(instr.arg[1]);
    auto rhs = _getDynRef(instr.arg[2]);

    assert(val_sizeof(dst) == 1);
    assert(val_sizeof(lhs) == val_sizeof(rhs));

    _getRef<int8_t>(instr.arg[0]) = std::memcmp(val_data(dst), val_data(lhs), val_sizeof(rhs)) != 0;
}

INTERP(ne_8) {
    _getRef<int8_t>(instr.arg[0]) =
        _getRef<uint8_t>(instr.arg[1]) != _getRef<uint8_t>(instr.arg[2]);
}

INTERP(ne_16) {
    _getRef<int8_t>(instr.arg[0]) =
        _getRef<uint16_t>(instr.arg[1]) != _getRef<uint16_t>(instr.arg[2]);
}

INTERP(ne_32) {
    _getRef<int8_t>(instr.arg[0]) =
        _getRef<uint32_t>(instr.arg[1]) != _getRef<uint32_t>(instr.arg[2]);
}

INTERP(ne_64) {
    _getRef<int8_t>(instr.arg[0]) =
        _getRef<uint64_t>(instr.arg[1]) != _getRef<uint64_t>(instr.arg[2]);
}

template <class F>
void _numericBinOp(Instr const &instr, F &&op) {
    auto dst = _getDynRef(instr.arg[0]);
    auto lhs = _getDynRef(instr.arg[1]);
    auto rhs = _getDynRef(instr.arg[2]);

    assert(val_typeid(dst) == val_typeid(lhs) && val_typeid(dst) == val_typeid(rhs));
    assert(val_typeclassid(dst) == Type_Numeric);

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

    assert(val_typeid(dst) == val_typeid(lhs) && val_typeid(dst) == val_typeid(rhs));
    assert(val_typeclassid(dst) == Type_Numeric && val_typeof(dst)->as.num.value_type < Float32);

    val_numeric_visit_int(dst, [&](auto &dst_val) {
        using T = std::decay_t<decltype(dst_val)>;
        dst_val = op(val_as(T, lhs), val_as(T, rhs));
    });
}

#define _NUM_BIN_OP_EXT(NAME, OP, EXT, TYPE)                                                      \
    INTERP(NAME##_##EXT) {                                                                        \
        _getRef<TYPE>(instr.arg[0]) = _getRef<TYPE>(instr.arg[1]) OP _getRef<TYPE>(instr.arg[2]); \
    }

#define NUM_BIN_OP(NAME, OP)                          \
    INTERP(NAME) {                                    \
        _numericBinOp(instr, [](auto lhs, auto rhs) { \
            return lhs OP rhs;                        \
        });                                           \
    }                                                 \
    _NUM_BIN_OP_EXT(NAME, OP, i8, int8_t)             \
    _NUM_BIN_OP_EXT(NAME, OP, u8, uint8_t)            \
    _NUM_BIN_OP_EXT(NAME, OP, i16, int16_t)           \
    _NUM_BIN_OP_EXT(NAME, OP, u16, uint16_t)          \
    _NUM_BIN_OP_EXT(NAME, OP, i32, int32_t)           \
    _NUM_BIN_OP_EXT(NAME, OP, u32, uint32_t)          \
    _NUM_BIN_OP_EXT(NAME, OP, i64, int64_t)           \
    _NUM_BIN_OP_EXT(NAME, OP, u64, uint64_t)          \
    _NUM_BIN_OP_EXT(NAME, OP, f32, float)             \
    _NUM_BIN_OP_EXT(NAME, OP, f64, double)

#define NUM_BIN_OP_INT(NAME, OP)                         \
    INTERP(NAME) {                                       \
        _numericBinOpInt(instr, [](auto lhs, auto rhs) { \
            return lhs OP rhs;                           \
        });                                              \
    }                                                    \
    _NUM_BIN_OP_EXT(NAME, OP, i8, int8_t)                \
    _NUM_BIN_OP_EXT(NAME, OP, u8, uint8_t)               \
    _NUM_BIN_OP_EXT(NAME, OP, i16, int16_t)              \
    _NUM_BIN_OP_EXT(NAME, OP, u16, uint16_t)             \
    _NUM_BIN_OP_EXT(NAME, OP, i32, int32_t)              \
    _NUM_BIN_OP_EXT(NAME, OP, u32, uint32_t)             \
    _NUM_BIN_OP_EXT(NAME, OP, i64, int64_t)              \
    _NUM_BIN_OP_EXT(NAME, OP, u64, uint64_t)

NUM_BIN_OP(add, +)
NUM_BIN_OP(sub, -)
NUM_BIN_OP(mul, *)
NUM_BIN_OP(div, /)
NUM_BIN_OP_INT(mod, %)

NUM_BIN_OP_INT(bitand, &)
NUM_BIN_OP_INT(bitor, |)
NUM_BIN_OP_INT(xor, ^)
NUM_BIN_OP_INT(lsh, <<)
NUM_BIN_OP_INT(rsh, >>)

NUM_BIN_OP(and, &&)
NUM_BIN_OP(or, ||)

NUM_BIN_OP(ge, >=)
NUM_BIN_OP(gt, >)
NUM_BIN_OP(le, <=)
NUM_BIN_OP(lt, <)

using InterpFunc = void (*)(Instr const &instr);

InterpFunc s_funcs[] = {
#define X(NAME) CAT(_interp_, NAME),
#include "nk/vm/op.inl"
};

} // namespace

void interp_invoke(type_t self, value_t ret, value_t args) {
    EASY_FUNCTION(profiler::colors::Blue200)

    LOG_TRC(__func__);

    EASY_NONSCOPED_BLOCK("setup", profiler::colors::Blue200)

    FunctInfo const &fn = *(FunctInfo *)self->as.fn.closure;
    Program const &prog = *fn.prog;

    LOG_DBG("program @%p", &prog);

    bool was_uninitialized = !ctx.is_initialized;
    if (was_uninitialized) {
        LOG_TRC("initializing stack...");
        //@Performance Choose ctx.stack reservation size, or remove initialiation at all
        ctx.stack.reserve(1024);
        ctx.is_initialized = true;
    }

    ProgramFrame pfr{
        .base_global = ctx.base.global,
        .base_rodata = ctx.base.rodata,
        .base_instr = ctx.base.instr,
        .base_reg = ctx.base.reg,
        .pinstr = ctx.pinstr,
    };

    ctx.base.global = prog.globals.data;
    ctx.base.rodata = prog.rodata.data;
    ctx.base.instr = (uint8_t *)prog.instrs.data;
    ctx.base.reg = (uint8_t *)&ctx.reg;
    ctx.pinstr = nullptr;

    LOG_DBG("global=%p", ctx.base.global);
    LOG_DBG("rodata=%p", ctx.base.rodata);
    LOG_DBG("instr=%p", ctx.base.instr);

    _jumpCall(fn.frame_t, ret, args, &prog.instrs[fn.first_instr]);

    EASY_END_BLOCK

    EASY_NONSCOPED_BLOCK("exec", profiler::colors::Blue200)

    while (ctx.pinstr) {
        auto pinstr = ctx.pinstr++;
        assert(pinstr->code < Op_Count && "unknown instruction");
        LOG_DBG(
            "instr: %lx %s", (pinstr - prog.instrs.data) * sizeof(Instr), s_op_names[pinstr->code]);
        s_funcs[pinstr->code](*pinstr);
        LOG_DBG("res=%s", [&]() {
            string str{};
            auto ref = pinstr->arg[0];
            if (ref.ref_type != bc::Ref_None) {
                static thread_local ARRAY_SLICE(char, buffer, 100);
                StaticStringBuilder sb{buffer};
                val_inspect(_getDynRef(ref), sb);
                sb << ":";
                type_name(ref.type, sb);
                str = sb.moveStr();
            }
            return str.data;
        }());
    }

    EASY_END_BLOCK

    EASY_NONSCOPED_BLOCK("teardown", profiler::colors::Blue200)

    LOG_TRC("exiting...");

    ctx.base.global = pfr.base_global;
    ctx.base.rodata = pfr.base_rodata;
    ctx.base.instr = pfr.base_instr;
    ctx.base.reg = pfr.base_reg;
    ctx.pinstr = pfr.pinstr;

    if (was_uninitialized) {
        LOG_TRC("deinitializing stack...");

        assert(ctx.stack.size() == 0 && "nonempty stack at exit");

        ctx.stack.deinit();
        ctx.ctrl_stack.deinit();

        ctx.is_initialized = false;
    }

    EASY_END_BLOCK
}

} // namespace vm
} // namespace nk
