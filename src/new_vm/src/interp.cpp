#include "interp.hpp"

#include <cassert>
#include <cstring>

#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/utils.hpp"
#include "nk/vm/value.h"

namespace {

NK_LOG_USE_SCOPE(interp);

struct ProgramFrame {
    uint8_t *base_global;
    uint8_t *base_rodata;
    uint8_t *base_instr;
    uint8_t *base_reg;
    NkBcInstr const *pinstr;
};

struct ControlFrame {
    NkStackAllocatorFrame stack_frame;
    uint8_t *base_frame;
    uint8_t *base_arg;
    uint8_t *base_ret;
    NkBcInstr const *pinstr;
};

struct InterpContext {
    struct Registers { // repeats the layout of NkIrRegister
        uint64_t a;
        uint64_t b;
        uint64_t c;
        uint64_t d;
        uint64_t e;
        uint64_t f;
    };

    struct Base { // repeats the layout of NkBcRefType
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
        uint8_t *base_ar[NkBcRef_Count];
        Base base;
    };
    NkStackAllocator *stack;
    std::vector<ControlFrame> ctrl_stack;
    std::vector<NkStackAllocatorFrame> stack_frames;
    NkStackAllocatorFrame stack_frame;
    NkBcInstr const *pinstr;
    bool is_initialized;
    Registers reg;
};

thread_local InterpContext ctx;

template <class T>
T &_getRef(NkBcRef ref) {
    auto ptr = ctx.base_ar[ref.ref_type] + ref.offset;
    if (ref.is_indirect) {
        ptr = *reinterpret_cast<uint8_t **>(ptr);
    }
    return *reinterpret_cast<T *>(ptr + ref.post_offset);
}

nkval_t _getDynRef(NkBcRef ref) {
    return {&_getRef<uint8_t>(ref), ref.type};
}

void _jumpTo(NkBcInstr const *pinstr) {
    NK_LOG_DBG("jumping to instr@%p", pinstr);
    ctx.pinstr = pinstr;
}

void _jumpTo(NkBcRef ref) {
    _jumpTo(&_getRef<NkBcInstr>(ref));
}

void _jumpCall(FunctInfo const &fn, nkval_t ret, nkval_t args) {
    ctx.ctrl_stack.emplace_back(ControlFrame{
        .stack_frame = ctx.stack_frame,
        .base_frame = ctx.base.frame,
        .base_arg = ctx.base.arg,
        .base_ret = ctx.base.ret,
        .pinstr = ctx.pinstr,
    });

    ctx.stack_frame = nk_stack_pushFrame(ctx.stack);
    ctx.base.frame = (uint8_t *)nk_stack_allocate(ctx.stack, fn.frame_size); // TODO not aligned
    ctx.base.arg = (uint8_t *)nkval_data(args);
    ctx.base.ret = (uint8_t *)nkval_data(ret);

    _jumpTo(&fn.prog->instrs[fn.first_instr]);

    NK_LOG_DBG("stack_frame=%lu", ctx.stack_frame.size);
    NK_LOG_DBG("frame=%p", ctx.base.frame);
    NK_LOG_DBG("arg=%p", ctx.base.arg);
    NK_LOG_DBG("ret=%p", ctx.base.ret);
    NK_LOG_DBG("pinstr=%p", ctx.pinstr);
}

#define INTERP(NAME) void _interp_##NAME(NkBcInstr const &instr)

INTERP(nop) {
    (void)instr;
}

INTERP(ret) {
    (void)instr;

    auto const fr = ctx.ctrl_stack.back();
    ctx.ctrl_stack.pop_back();

    nk_stack_popFrame(ctx.stack, ctx.stack_frame);

    ctx.stack_frame = fr.stack_frame;
    ctx.base.frame = fr.base_frame;
    ctx.base.arg = fr.base_arg;
    ctx.base.ret = fr.base_ret;

    _jumpTo(fr.pinstr);
}

INTERP(enter) {
    (void)instr;

    ctx.stack_frames.emplace_back(nk_stack_pushFrame(ctx.stack));
}

INTERP(leave) {
    (void)instr;

    nk_stack_popFrame(ctx.stack, ctx.stack_frames.back());
    ctx.stack_frames.pop_back();
}

INTERP(jmp) {
    _jumpTo(instr.arg[1]);
}

INTERP(jmpz) {
    auto cond = _getDynRef(instr.arg[1]);

    assert(nkval_typeclassid(cond) == NkType_Numeric);

    // TODO val_numeric_visit(cond, [=](auto val) {
    //     if (!val) {
    //         _jumpTo(instr.arg[2]);
    //     }
    // });
}

INTERP(jmpnz) {
    auto cond = _getDynRef(instr.arg[1]);

    assert(nkval_typeclassid(cond) == NkType_Numeric);

    // TODO val_numeric_visit(cond, [=](auto val) {
    //     if (val) {
    //         _jumpTo(instr.arg[2]);
    //     }
    // });
}

INTERP(cast) {
    // TODO auto dst = _getDynRef(instr.arg[0]);
    // auto type = _getDynRef(instr.arg[1]);
    // auto arg = _getDynRef(instr.arg[2]);

    // assert(nkval_typeclassid(dst) == NkType_Numeric);
    // assert(nkval_typeclassid(type) == Type_Ptr);
    // assert(nkval_typeid(dst) == nkval_as(type_t, type)->id);
    // assert(nkval_typeclassid(arg) == NkType_Numeric);

    // val_numeric_visit(dst, [&](auto &dst_val) {
    //     using T = std::decay_t<decltype(dst_val)>;
    //     val_numeric_visit(arg, [&](auto arg_val) {
    //         dst_val = static_cast<T>(arg_val);
    //     });
    // });
}

INTERP(call) {
    auto ret = _getDynRef(instr.arg[0]);
    auto fn_val = _getDynRef(instr.arg[1]);
    auto args = _getDynRef(instr.arg[2]);

    // TODO val_fn_invoke(nkval_typeof(fn_val), ret, args);
}

INTERP(call_jmp) {
    auto ret = _getDynRef(instr.arg[0]);
    auto fn_val = _getDynRef(instr.arg[1]);
    auto args = _getDynRef(instr.arg[2]);

    FunctInfo const &fn = *(FunctInfo *)nkval_typeof(fn_val)->as.fn.closure;
    _jumpCall(fn, ret, args);
}

INTERP(mov) {
    auto dst = _getDynRef(instr.arg[0]);
    auto src = _getDynRef(instr.arg[1]);

    assert(nkval_sizeof(dst) == nkval_sizeof(src));

    std::memcpy(nkval_data(dst), nkval_data(src), nkval_sizeof(dst));
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

    assert(nkval_typeid(dst) == nkval_typeid(arg));
    assert(nkval_typeclassid(dst) == NkType_Numeric);

    // TODO val_numeric_visit(dst, [&](auto &dst_val) {
    //     using T = std::decay_t<decltype(dst_val)>;
    //     dst_val = -nkval_as(T, arg);
    // });
}

INTERP(compl ) {
    auto dst = _getDynRef(instr.arg[0]);
    auto arg = _getDynRef(instr.arg[1]);

    assert(nkval_typeid(dst) == nkval_typeid(arg));
    assert(
        nkval_typeclassid(dst) == NkType_Numeric && nkval_typeof(dst)->as.num.value_type < Float32);

    // TODO val_numeric_visit_int(dst, [&](auto &dst_val) {
    //     using T = std::decay_t<decltype(dst_val)>;
    //     dst_val = ~nkval_as(T, arg);
    // });
}

INTERP(not ) {
    auto dst = _getDynRef(instr.arg[0]);
    auto arg = _getDynRef(instr.arg[1]);

    assert(nkval_typeid(dst) == nkval_typeid(arg));
    assert(nkval_typeclassid(dst) == NkType_Numeric);

    // TODO val_numeric_visit(dst, [&](auto &dst_val) {
    //     using T = std::decay_t<decltype(dst_val)>;
    //     dst_val = !nkval_as(T, arg);
    // });
}

INTERP(eq) {
    auto dst = _getDynRef(instr.arg[0]);
    auto lhs = _getDynRef(instr.arg[1]);
    auto rhs = _getDynRef(instr.arg[2]);

    assert(nkval_sizeof(dst) == 1);
    assert(nkval_sizeof(lhs) == nkval_sizeof(rhs));

    _getRef<int8_t>(instr.arg[0]) =
        std::memcmp(nkval_data(dst), nkval_data(lhs), nkval_sizeof(rhs)) == 0;
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

    assert(nkval_sizeof(dst) == 1);
    assert(nkval_sizeof(lhs) == nkval_sizeof(rhs));

    _getRef<int8_t>(instr.arg[0]) =
        std::memcmp(nkval_data(dst), nkval_data(lhs), nkval_sizeof(rhs)) != 0;
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
void _numericBinBc(NkBcInstr const &instr, F &&op) {
    auto dst = _getDynRef(instr.arg[0]);
    auto lhs = _getDynRef(instr.arg[1]);
    auto rhs = _getDynRef(instr.arg[2]);

    assert(nkval_typeid(dst) == nkval_typeid(lhs) && nkval_typeid(dst) == nkval_typeid(rhs));
    assert(nkval_typeclassid(dst) == NkType_Numeric);

    // TODO val_numeric_visit(dst, [&](auto &dst_val) {
    //     using T = std::decay_t<decltype(dst_val)>;
    //     dst_val = op(nkval_as(T, lhs), nkval_as(T, rhs));
    // });
}

template <class F>
void _numericBinBcInt(NkBcInstr const &instr, F &&op) {
    auto dst = _getDynRef(instr.arg[0]);
    auto lhs = _getDynRef(instr.arg[1]);
    auto rhs = _getDynRef(instr.arg[2]);

    assert(nkval_typeid(dst) == nkval_typeid(lhs) && nkval_typeid(dst) == nkval_typeid(rhs));
    assert(
        nkval_typeclassid(dst) == NkType_Numeric && nkval_typeof(dst)->as.num.value_type < Float32);

    // TODO val_numeric_visit_int(dst, [&](auto &dst_val) {
    //     using T = std::decay_t<decltype(dst_val)>;
    //     dst_val = op(nkval_as(T, lhs), nkval_as(T, rhs));
    // });
}

#define _NUM_BIN_OP_EXT(NAME, OP, EXT, TYPE)                                                      \
    INTERP(NAME##_##EXT) {                                                                        \
        _getRef<TYPE>(instr.arg[0]) = _getRef<TYPE>(instr.arg[1]) OP _getRef<TYPE>(instr.arg[2]); \
    }

#define NUM_BIN_OP(NAME, OP)                          \
    INTERP(NAME) {                                    \
        _numericBinBc(instr, [](auto lhs, auto rhs) { \
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
        _numericBinBcInt(instr, [](auto lhs, auto rhs) { \
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

using InterpFunc = void (*)(NkBcInstr const &instr);

InterpFunc s_funcs[] = {
#define X(NAME) CAT(_interp_, NAME),
#include "bytecode.inl"
};

} // namespace

void nk_interp_invoke(FunctInfo const &fn, nkval_t ret, nkval_t args) {
    NK_LOG_TRC(__func__);

    auto const &prog = *fn.prog;

    NK_LOG_DBG("program @%p", &prog);

    bool was_uninitialized = !ctx.is_initialized;
    if (was_uninitialized) {
        NK_LOG_TRC("initializing stack...");
        // TODO ctx.stack.reserve(1024);
        ctx.stack = nk_create_stack();
        ctx.is_initialized = true;
    }

    ProgramFrame pfr{
        .base_global = ctx.base.global,
        .base_rodata = ctx.base.rodata,
        .base_instr = ctx.base.instr,
        .base_reg = ctx.base.reg,
        .pinstr = ctx.pinstr,
    };

    ctx.base.global = 0; // TODO prog.globals.data;
    ctx.base.rodata = 0; // TODO prog.rodata.data;
    ctx.base.instr = (uint8_t *)prog.instrs.data();
    ctx.base.reg = (uint8_t *)&ctx.reg;
    ctx.pinstr = nullptr;

    NK_LOG_DBG("global=%p", ctx.base.global);
    NK_LOG_DBG("rodata=%p", ctx.base.rodata);
    NK_LOG_DBG("instr=%p", ctx.base.instr);

    _jumpCall(fn, ret, args);

    while (ctx.pinstr) {
        auto pinstr = ctx.pinstr++;
        assert(pinstr->code < nkop_count && "unknown instruction");
        NK_LOG_DBG(
            "instr: %lx %s",
            (pinstr - prog.instrs.data()) * sizeof(NkBcInstr),
            s_nk_bc_names[pinstr->code]);
        s_funcs[pinstr->code](*pinstr);
        // TODO NK_LOG_DBG("res=%s", [&]() {
        //     string str{};
        //     auto ref = pinstr->arg[0];
        //     if (ref.ref_type != bc::Ref_None) {
        //         static thread_local ARRAY_SLICE(char, buffer, 100);
        //         StaticStringBuilder sb{buffer};
        //         val_inspect(_getDynRef(ref), sb);
        //         sb << ":";
        //         types::inspect(ref.type, sb);
        //         str = sb.moveStr();
        //     }
        //     return str.data;
        // }());
    }

    NK_LOG_TRC("exiting...");

    ctx.base.global = pfr.base_global;
    ctx.base.rodata = pfr.base_rodata;
    ctx.base.instr = pfr.base_instr;
    ctx.base.reg = pfr.base_reg;
    ctx.pinstr = pfr.pinstr;

    if (was_uninitialized) {
        NK_LOG_TRC("deinitializing stack...");

        // TODO assert(ctx.stack.size() == 0 && "nonempty stack at exit");

        nk_free_stack(ctx.stack);

        ctx.is_initialized = false;
    }
}
