#include "interp.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

#include "bytecode.h"
#include "bytecode_impl.hpp"
#include "ir_impl.hpp"
#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nk/common/string_builder.h"
#include "nk/common/utils.h"
#include "nk/common/utils.hpp"
#include "nk/vm/value.h"

namespace {

NK_LOG_USE_SCOPE(interp);

struct ProgramFrame {
    uint8_t *base_reg;
    NkBcInstr const *pinstr;
};

struct ControlFrame {
    NkStackAllocatorFrame stack_frame;
    uint8_t *base_frame;
    uint8_t *base_arg;
    uint8_t *base_ret;
    uint8_t *base_instr;
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
        uint8_t *none;
        uint8_t *frame;
        uint8_t *arg;
        uint8_t *ret;
        uint8_t *reg;
        uint8_t *rodata;
        uint8_t *data;
        uint8_t *instr;
    };

    union {
        uint8_t *base_ar[NkBcRef_Count];
        Base base;
    };
    NkStackAllocator stack;
    std::vector<ControlFrame> ctrl_stack;
    std::vector<NkStackAllocatorFrame> stack_frames;
    NkStackAllocatorFrame stack_frame;
    NkBcInstr const *pinstr;
    Registers reg;
    bool is_initialized;
};

thread_local InterpContext ctx;

thread_local auto s_deinit_ctx = makeDeferrer([]() {
    NK_LOG_TRC("deinitializing stack...");
    assert(nk_stack_getFrame(ctx.stack).size == 0 && "nonempty stack at exit");
    nk_free_stack(ctx.stack);
    ctx.is_initialized = false;
});

template <class T>
T &_getRef(NkBcRef const &ref) {
    auto ptr = ctx.base_ar[ref.ref_type] + ref.offset;
    if (ref.is_indirect) {
        ptr = *(uint8_t **)ptr;
    }
    return *(T *)(ptr + ref.post_offset);
}

nkval_t _getValRef(NkBcRef const &ref) {
    return {&_getRef<uint8_t>(ref), ref.type};
}

void _jumpTo(NkBcInstr const *pinstr) {
    NK_LOG_DBG("jumping to instr@%p", pinstr);
    ctx.pinstr = pinstr;
}

void _jumpTo(NkBcRef const &ref) {
    _jumpTo(&_getRef<NkBcInstr>(ref));
}

void _jumpCall(NkBcFunct fn, nkval_t ret, nkval_t args) {
    ctx.ctrl_stack.emplace_back(ControlFrame{
        .stack_frame = ctx.stack_frame,
        .base_frame = ctx.base.frame,
        .base_arg = ctx.base.arg,
        .base_ret = ctx.base.ret,
        .base_instr = ctx.base.instr,
        .pinstr = ctx.pinstr,
    });

    ctx.stack_frame = nk_stack_getFrame(ctx.stack);
    ctx.base.frame = (uint8_t *)nk_stack_allocate(ctx.stack, fn->frame_size); // TODO not aligned
    std::memset(ctx.base.frame, 0, fn->frame_size);
    ctx.base.arg = (uint8_t *)nkval_data(args);
    ctx.base.ret = (uint8_t *)nkval_data(ret);
    ctx.base.instr = (uint8_t *)fn->instrs;

    _jumpTo(fn->instrs);

    NK_LOG_DBG("stack_frame=%lu", ctx.stack_frame.size);
    NK_LOG_DBG("frame=%p", ctx.base.frame);
    NK_LOG_DBG("arg=%p", ctx.base.arg);
    NK_LOG_DBG("ret=%p", ctx.base.ret);
    NK_LOG_DBG("pinstr=%p", ctx.pinstr);
}

void interp(NkBcInstr const &instr) {
    switch (instr.code) {
    case nkop_nop: {
        break;
    }

    case nkop_ret: {
        auto const fr = ctx.ctrl_stack.back();
        ctx.ctrl_stack.pop_back();

        nk_stack_popFrame(ctx.stack, ctx.stack_frame);

        ctx.stack_frame = fr.stack_frame;
        ctx.base.frame = fr.base_frame;
        ctx.base.arg = fr.base_arg;
        ctx.base.ret = fr.base_ret;
        ctx.base.instr = fr.base_instr;

        _jumpTo(fr.pinstr);
        break;
    }

    case nkop_enter: {
        ctx.stack_frames.emplace_back(nk_stack_getFrame(ctx.stack));
        break;
    }

    case nkop_leave: {
        nk_stack_popFrame(ctx.stack, ctx.stack_frames.back());
        ctx.stack_frames.pop_back();
        break;
    }

    case nkop_jmp: {
        _jumpTo(instr.arg[1]);
        break;
    }

    case nkop_jmpz: {
        assert(!"not implemented");
        break;
    }

    case nkop_jmpz_8: {
        if (!_getRef<uint8_t>(instr.arg[1])) {
            _jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpz_16: {
        if (!_getRef<uint16_t>(instr.arg[1])) {
            _jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpz_32: {
        if (!_getRef<uint32_t>(instr.arg[1])) {
            _jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpz_64: {
        if (!_getRef<uint64_t>(instr.arg[1])) {
            _jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpnz: {
        assert(!"not implemented");
        break;
    }

    case nkop_jmpnz_8: {
        if (_getRef<uint8_t>(instr.arg[1])) {
            _jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpnz_16: {
        if (_getRef<uint16_t>(instr.arg[1])) {
            _jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpnz_32: {
        if (_getRef<uint32_t>(instr.arg[1])) {
            _jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpnz_64: {
        if (_getRef<uint64_t>(instr.arg[1])) {
            _jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_cast: {
        assert(!"generic cast not implemented");
        break;
    }

#define _CAST(FROM_NAME, FROM_TYPE, TO_NAME, TO_TYPE)                               \
    case CAT(CAT(CAT(nkop_cast_, FROM_NAME), _to_), TO_NAME): {                     \
        _getRef<TO_TYPE>(instr.arg[0]) = (TO_TYPE)_getRef<FROM_TYPE>(instr.arg[2]); \
        break;                                                                      \
    }

#define CAST(TO_NAME, TO_TYPE)             \
    _CAST(i8, int8_t, TO_NAME, TO_TYPE)    \
    _CAST(u8, uint8_t, TO_NAME, TO_TYPE)   \
    _CAST(i16, int16_t, TO_NAME, TO_TYPE)  \
    _CAST(u16, uint16_t, TO_NAME, TO_TYPE) \
    _CAST(i32, int32_t, TO_NAME, TO_TYPE)  \
    _CAST(u32, uint32_t, TO_NAME, TO_TYPE) \
    _CAST(i64, int64_t, TO_NAME, TO_TYPE)  \
    _CAST(u64, uint64_t, TO_NAME, TO_TYPE) \
    _CAST(f32, float, TO_NAME, TO_TYPE)    \
    _CAST(f64, double, TO_NAME, TO_TYPE)

        CAST(i8, int8_t)
        CAST(u8, uint8_t)
        CAST(i16, int16_t)
        CAST(u16, uint16_t)
        CAST(i32, int32_t)
        CAST(u32, uint32_t)
        CAST(i64, int64_t)
        CAST(u64, uint64_t)
        CAST(f32, float)
        CAST(f64, double)

#undef CAST
#undef _CAST

    case nkop_call: {
        auto ret = _getValRef(instr.arg[0]);
        auto fn_val = _getValRef(instr.arg[1]);
        auto args = _getValRef(instr.arg[2]);

        nkval_fn_invoke(fn_val, ret, args);
        break;
    }

    case nkop_call_jmp: {
        auto ret = _getValRef(instr.arg[0]);
        auto fn_val = _getValRef(instr.arg[1]);
        auto args = _getValRef(instr.arg[2]);

        _jumpCall(nkval_as(NkIrFunct, fn_val)->bc_funct, ret, args);
        break;
    }

    case nkop_mov: {
        auto dst = _getValRef(instr.arg[0]);
        auto src = _getValRef(instr.arg[1]);

        assert(nkval_sizeof(dst) == nkval_sizeof(src));

        std::memcpy(nkval_data(dst), nkval_data(src), nkval_sizeof(dst));
        break;
    }

    case nkop_mov_8: {
        _getRef<uint8_t>(instr.arg[0]) = _getRef<uint8_t>(instr.arg[1]);
        break;
    }

    case nkop_mov_16: {
        _getRef<uint16_t>(instr.arg[0]) = _getRef<uint16_t>(instr.arg[1]);
        break;
    }

    case nkop_mov_32: {
        _getRef<uint32_t>(instr.arg[0]) = _getRef<uint32_t>(instr.arg[1]);
        break;
    }

    case nkop_mov_64: {
        _getRef<uint64_t>(instr.arg[0]) = _getRef<uint64_t>(instr.arg[1]);
        break;
    }

    case nkop_lea: {
        _getRef<void *>(instr.arg[0]) = &_getRef<uint8_t>(instr.arg[1]);
        break;
    }

#define NUM_UN_OP_EXT(NAME, OP, EXT, TYPE)                            \
    case CAT(CAT(CAT(nkop_, NAME), _), EXT): {                        \
        _getRef<TYPE>(instr.arg[0]) = OP _getRef<TYPE>(instr.arg[1]); \
        break;                                                        \
    }

    // TODO Optimize macros in interp

#define NUM_UN_OP(NAME, OP)                          \
    case CAT(nkop_, NAME): {                         \
        assert(!"generic un op is not implemented"); \
        break;                                       \
    }                                                \
        NUM_UN_OP_EXT(NAME, OP, i8, int8_t)          \
        NUM_UN_OP_EXT(NAME, OP, u8, uint8_t)         \
        NUM_UN_OP_EXT(NAME, OP, i16, int16_t)        \
        NUM_UN_OP_EXT(NAME, OP, u16, uint16_t)       \
        NUM_UN_OP_EXT(NAME, OP, i32, int32_t)        \
        NUM_UN_OP_EXT(NAME, OP, u32, uint32_t)       \
        NUM_UN_OP_EXT(NAME, OP, i64, int64_t)        \
        NUM_UN_OP_EXT(NAME, OP, u64, uint64_t)       \
        NUM_UN_OP_EXT(NAME, OP, f32, float)          \
        NUM_UN_OP_EXT(NAME, OP, f64, double)

#define NUM_UN_OP_INT(NAME, OP)                      \
    case CAT(nkop_, NAME): {                         \
        assert(!"generic un op is not implemented"); \
        break;                                       \
    }                                                \
        NUM_UN_OP_EXT(NAME, OP, i8, int8_t)          \
        NUM_UN_OP_EXT(NAME, OP, u8, uint8_t)         \
        NUM_UN_OP_EXT(NAME, OP, i16, int16_t)        \
        NUM_UN_OP_EXT(NAME, OP, u16, uint16_t)       \
        NUM_UN_OP_EXT(NAME, OP, i32, int32_t)        \
        NUM_UN_OP_EXT(NAME, OP, u32, uint32_t)       \
        NUM_UN_OP_EXT(NAME, OP, i64, int64_t)        \
        NUM_UN_OP_EXT(NAME, OP, u64, uint64_t)

        NUM_UN_OP(neg, -)
        NUM_UN_OP(not, !)

        NUM_UN_OP_INT(compl, ~)

#undef NUM_UN_OP
#undef NUM_UN_OP_INT
#undef NUM_UN_OP_EXT

    case nkop_eq: {
        auto dst = _getValRef(instr.arg[0]);
        auto lhs = _getValRef(instr.arg[1]);
        auto rhs = _getValRef(instr.arg[2]);

        assert(nkval_sizeof(dst) == 1);
        assert(nkval_sizeof(lhs) == nkval_sizeof(rhs));

        _getRef<uint8_t>(instr.arg[0]) =
            std::memcmp(nkval_data(dst), nkval_data(lhs), nkval_sizeof(rhs)) == 0;
        break;
    }

    case nkop_eq_8: {
        _getRef<uint8_t>(instr.arg[0]) =
            _getRef<uint8_t>(instr.arg[1]) == _getRef<uint8_t>(instr.arg[2]);
        break;
    }

    case nkop_eq_16: {
        _getRef<uint8_t>(instr.arg[0]) =
            _getRef<uint16_t>(instr.arg[1]) == _getRef<uint16_t>(instr.arg[2]);
        break;
    }

    case nkop_eq_32: {
        _getRef<uint8_t>(instr.arg[0]) =
            _getRef<uint32_t>(instr.arg[1]) == _getRef<uint32_t>(instr.arg[2]);
        break;
    }

    case nkop_eq_64: {
        _getRef<uint8_t>(instr.arg[0]) =
            _getRef<uint64_t>(instr.arg[1]) == _getRef<uint64_t>(instr.arg[2]);
        break;
    }

    case nkop_ne: {
        auto dst = _getValRef(instr.arg[0]);
        auto lhs = _getValRef(instr.arg[1]);
        auto rhs = _getValRef(instr.arg[2]);

        assert(nkval_sizeof(dst) == 1);
        assert(nkval_sizeof(lhs) == nkval_sizeof(rhs));

        _getRef<uint8_t>(instr.arg[0]) =
            std::memcmp(nkval_data(dst), nkval_data(lhs), nkval_sizeof(rhs)) != 0;
        break;
    }

    case nkop_ne_8: {
        _getRef<uint8_t>(instr.arg[0]) =
            _getRef<uint8_t>(instr.arg[1]) != _getRef<uint8_t>(instr.arg[2]);
        break;
    }

    case nkop_ne_16: {
        _getRef<uint8_t>(instr.arg[0]) =
            _getRef<uint16_t>(instr.arg[1]) != _getRef<uint16_t>(instr.arg[2]);
        break;
    }

    case nkop_ne_32: {
        _getRef<uint8_t>(instr.arg[0]) =
            _getRef<uint32_t>(instr.arg[1]) != _getRef<uint32_t>(instr.arg[2]);
        break;
    }

    case nkop_ne_64: {
        _getRef<uint8_t>(instr.arg[0]) =
            _getRef<uint64_t>(instr.arg[1]) != _getRef<uint64_t>(instr.arg[2]);
        break;
    }

#define NUM_BIN_OP_EXT(NAME, OP, EXT, TYPE)                                                       \
    case CAT(CAT(CAT(nkop_, NAME), _), EXT): {                                                    \
        _getRef<TYPE>(instr.arg[0]) = _getRef<TYPE>(instr.arg[1]) OP _getRef<TYPE>(instr.arg[2]); \
        break;                                                                                    \
    }

#define NUM_BIN_BOOL_OP_EXT(NAME, OP, EXT, TYPE)                        \
    case CAT(CAT(CAT(nkop_, NAME), _), EXT): {                          \
        _getRef<uint8_t>(instr.arg[0]) =                                \
            _getRef<TYPE>(instr.arg[1]) OP _getRef<TYPE>(instr.arg[2]); \
        break;                                                          \
    }

#define NUM_BIN_OP(NAME, OP)                          \
    case CAT(nkop_, NAME): {                          \
        assert(!"generic bin op is not implemented"); \
        break;                                        \
    }                                                 \
        NUM_BIN_OP_EXT(NAME, OP, i8, int8_t)          \
        NUM_BIN_OP_EXT(NAME, OP, u8, uint8_t)         \
        NUM_BIN_OP_EXT(NAME, OP, i16, int16_t)        \
        NUM_BIN_OP_EXT(NAME, OP, u16, uint16_t)       \
        NUM_BIN_OP_EXT(NAME, OP, i32, int32_t)        \
        NUM_BIN_OP_EXT(NAME, OP, u32, uint32_t)       \
        NUM_BIN_OP_EXT(NAME, OP, i64, int64_t)        \
        NUM_BIN_OP_EXT(NAME, OP, u64, uint64_t)       \
        NUM_BIN_OP_EXT(NAME, OP, f32, float)          \
        NUM_BIN_OP_EXT(NAME, OP, f64, double)

#define NUM_BIN_BOOL_OP(NAME, OP)                          \
    case CAT(nkop_, NAME): {                               \
        assert(!"generic bin bool op is not implemented"); \
        break;                                             \
    }                                                      \
        NUM_BIN_BOOL_OP_EXT(NAME, OP, i8, int8_t)          \
        NUM_BIN_BOOL_OP_EXT(NAME, OP, u8, uint8_t)         \
        NUM_BIN_BOOL_OP_EXT(NAME, OP, i16, int16_t)        \
        NUM_BIN_BOOL_OP_EXT(NAME, OP, u16, uint16_t)       \
        NUM_BIN_BOOL_OP_EXT(NAME, OP, i32, int32_t)        \
        NUM_BIN_BOOL_OP_EXT(NAME, OP, u32, uint32_t)       \
        NUM_BIN_BOOL_OP_EXT(NAME, OP, i64, int64_t)        \
        NUM_BIN_BOOL_OP_EXT(NAME, OP, u64, uint64_t)       \
        NUM_BIN_BOOL_OP_EXT(NAME, OP, f32, float)          \
        NUM_BIN_BOOL_OP_EXT(NAME, OP, f64, double)

#define NUM_BIN_OP_INT(NAME, OP)                      \
    case CAT(nkop_, NAME): {                          \
        assert(!"generic bin op is not implemented"); \
        break;                                        \
    }                                                 \
        NUM_BIN_OP_EXT(NAME, OP, i8, int8_t)          \
        NUM_BIN_OP_EXT(NAME, OP, u8, uint8_t)         \
        NUM_BIN_OP_EXT(NAME, OP, i16, int16_t)        \
        NUM_BIN_OP_EXT(NAME, OP, u16, uint16_t)       \
        NUM_BIN_OP_EXT(NAME, OP, i32, int32_t)        \
        NUM_BIN_OP_EXT(NAME, OP, u32, uint32_t)       \
        NUM_BIN_OP_EXT(NAME, OP, i64, int64_t)        \
        NUM_BIN_OP_EXT(NAME, OP, u64, uint64_t)

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

        NUM_BIN_BOOL_OP(and, &&)
        NUM_BIN_BOOL_OP(or, ||)

        NUM_BIN_BOOL_OP(ge, >=)
        NUM_BIN_BOOL_OP(gt, >)
        NUM_BIN_BOOL_OP(le, <=)
        NUM_BIN_BOOL_OP(lt, <)

#undef NUM_BIN_OP
#undef NUM_BIN_OP_INT
#undef NUM_BIN_OP_EXT
#undef NUM_BIN_BOOL_OP
#undef NUM_BIN_BOOL_OP_EXT

    default:
        assert(!"unknown opcode");
        break;
    }
}

} // namespace

void nk_interp_invoke(NkBcFunct fn, nkval_t ret, nkval_t args) {
    EASY_FUNCTION(::profiler::colors::Red200);

    NK_LOG_TRC(__func__);

    auto const &prog = *fn->prog;

    NK_LOG_DBG("program @%p", &prog);

    if (!ctx.is_initialized) {
        NK_LOG_TRC("initializing stack...");
        // TODO ctx.stack.reserve(1024);
        ctx.stack = nk_create_stack();
        ctx.is_initialized = true;
    }

    ProgramFrame pfr{
        .base_reg = ctx.base.reg,
        .pinstr = ctx.pinstr,
    };

    ctx.base.reg = (uint8_t *)&ctx.reg;
    ctx.pinstr = nullptr;

    NK_LOG_DBG("instr=%p", ctx.base.instr);

    _jumpCall(fn, ret, args);

    while (ctx.pinstr) {
        auto pinstr = ctx.pinstr++;
        assert(pinstr->code < nkop_count && "unknown instruction");
        NK_LOG_DBG(
            "instr: %lx %s",
            (pinstr - (NkBcInstr *)ctx.base.instr) * sizeof(NkBcInstr),
            s_nk_bc_names[pinstr->code]);
        interp(*pinstr);
        NK_LOG_DBG(
            "res=%s", (char const *)[&]() {
                NkStringBuilder sb{};
                char const *str{};
                auto const &ref = pinstr->arg[0];
                if (ref.ref_type != NkBcRef_None) {
                    sb = nksb_create();
                    nkval_inspect(_getValRef(ref), sb);
                    nksb_printf(sb, ":");
                    nkt_inspect(ref.type, sb);
                    str = nksb_concat(sb).data;
                }
                return makeDeferrerWithData(str, [sb]() {
                    nksb_free(sb);
                });
            }());
    }

    NK_LOG_TRC("exiting...");

    ctx.base.reg = pfr.base_reg;
    ctx.pinstr = pfr.pinstr;
}
