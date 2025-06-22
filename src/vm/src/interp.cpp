#include "interp.hpp"

#include <cstring>

#include "bytecode.h"
#include "bytecode_impl.hpp"
#include "ir_impl.hpp"
#include "nk/vm/value.h"
#include "ntk/allocator.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(interp);

struct ProgramFrame {
    u8 *base_reg;
    NkBcInstr const *pinstr;
};

struct ControlFrame {
    NkArenaFrame stack_frame;
    u8 *base_frame;
    u8 *base_arg;
    u8 *base_ret;
    u8 *base_instr;
    NkBcInstr const *pinstr;
};

struct InterpContext {
    struct Registers { // repeats the layout of NkIrRegister
        u64 a;
        u64 b;
        u64 c;
        u64 d;
        u64 e;
        u64 f;
    };

    struct Base { // repeats the layout of NkBcRefType
        u8 *none;
        u8 *frame;
        u8 *arg;
        u8 *ret;
        u8 *reg;
        u8 *rodata;
        u8 *data;
        u8 *instr;
    };

    union {
        u8 *base_ar[NkBcRef_Count];
        Base base;
    };
    NkArena stack;
    std::vector<ControlFrame> ctrl_stack;
    std::vector<NkArenaFrame> stack_frames;
    NkArenaFrame stack_frame;
    NkBcInstr const *pinstr;
    Registers reg;
    bool is_initialized;

    ~InterpContext() {
        NK_LOG_TRC("deinitializing stack...");
        nk_assert(nk_arena_grab(&stack).size == 0 && "nonempty stack at exit");
        nk_arena_free(&stack);
        is_initialized = false;
    }
};

thread_local InterpContext ctx;

template <class T>
T &_getRef(NkBcRef const &ref) {
    auto ptr = ctx.base_ar[ref.ref_type] + ref.offset;
    if (ref.is_indirect) {
        ptr = *(u8 **)ptr;
    }
    return *(T *)(ptr + ref.post_offset);
}

nkval_t _getValRef(NkBcRef const &ref) {
    return {&_getRef<u8>(ref), ref.type};
}

void _jumpTo(NkBcInstr const *pinstr) {
    NK_LOG_DBG("jumping to instr@%p", (void *)pinstr);
    ctx.pinstr = pinstr;
}

void _jumpTo(NkBcRef const &ref) {
    _jumpTo(&_getRef<NkBcInstr>(ref));
}

void _jumpCall(NkBcFunct fn, nkval_t ret, nkval_t args) {
    ctx.ctrl_stack.emplace_back(
        ControlFrame{
            .stack_frame = ctx.stack_frame,
            .base_frame = ctx.base.frame,
            .base_arg = ctx.base.arg,
            .base_ret = ctx.base.ret,
            .base_instr = ctx.base.instr,
            .pinstr = ctx.pinstr,
        });

    ctx.stack_frame = nk_arena_grab(&ctx.stack);
    ctx.base.frame = (u8 *)nk_arena_allocAligned(&ctx.stack, fn->frame_size, alignof(max_align_t));
    std::memset(ctx.base.frame, 0, fn->frame_size);
    ctx.base.arg = (u8 *)nkval_data(args);
    ctx.base.ret = (u8 *)nkval_data(ret);
    ctx.base.instr = (u8 *)fn->instrs;

    _jumpTo(fn->instrs);

    NK_LOG_DBG("stack_frame=%zu", ctx.stack_frame.size);
    NK_LOG_DBG("frame=%p", (void *)ctx.base.frame);
    NK_LOG_DBG("arg=%p", (void *)ctx.base.arg);
    NK_LOG_DBG("ret=%p", (void *)ctx.base.ret);
    NK_LOG_DBG("pinstr=%p", (void *)ctx.pinstr);
}

void interp(NkBcInstr const &instr) {
#ifdef ENABLE_PROFILING
    NKSB_FIXED_BUFFER(sb, 128);
    nksb_printf(&sb, "interp: %s", s_nk_bc_names[instr.code]);
#endif // ENABLE_PROFILING
    NK_PROF_SCOPE(sb);

    switch (instr.code) {
        case nkop_nop: {
            break;
        }

        case nkop_ret: {
            auto const fr = ctx.ctrl_stack.back();
            ctx.ctrl_stack.pop_back();

            nk_arena_popFrame(&ctx.stack, ctx.stack_frame);

            ctx.stack_frame = fr.stack_frame;
            ctx.base.frame = fr.base_frame;
            ctx.base.arg = fr.base_arg;
            ctx.base.ret = fr.base_ret;
            ctx.base.instr = fr.base_instr;

            _jumpTo(fr.pinstr);
            break;
        }

        case nkop_enter: {
            // ctx.stack_frames.emplace_back(nk_arena_grab(&ctx.stack));
            break;
        }

        case nkop_leave: {
            // nk_arena_popFrame(&ctx.stack, ctx.stack_frames.back());
            // ctx.stack_frames.pop_back();
            break;
        }

        case nkop_jmp: {
            _jumpTo(instr.arg[1]);
            break;
        }

        case nkop_jmpz_8: {
            if (!_getRef<u8>(instr.arg[1])) {
                _jumpTo(instr.arg[2]);
            }
            break;
        }

        case nkop_jmpz_16: {
            if (!_getRef<u16>(instr.arg[1])) {
                _jumpTo(instr.arg[2]);
            }
            break;
        }

        case nkop_jmpz_32: {
            if (!_getRef<u32>(instr.arg[1])) {
                _jumpTo(instr.arg[2]);
            }
            break;
        }

        case nkop_jmpz_64: {
            if (!_getRef<u64>(instr.arg[1])) {
                _jumpTo(instr.arg[2]);
            }
            break;
        }

        case nkop_jmpnz_8: {
            if (_getRef<u8>(instr.arg[1])) {
                _jumpTo(instr.arg[2]);
            }
            break;
        }

        case nkop_jmpnz_16: {
            if (_getRef<u16>(instr.arg[1])) {
                _jumpTo(instr.arg[2]);
            }
            break;
        }

        case nkop_jmpnz_32: {
            if (_getRef<u32>(instr.arg[1])) {
                _jumpTo(instr.arg[2]);
            }
            break;
        }

        case nkop_jmpnz_64: {
            if (_getRef<u64>(instr.arg[1])) {
                _jumpTo(instr.arg[2]);
            }
            break;
        }

#define _CAST(FROM_TYPE, FROM_VALUE_TYPE, TO_TYPE)                                  \
    case NK_CAT(NK_CAT(NK_CAT(nkop_cast_, FROM_TYPE), _to_), TO_TYPE): {            \
        _getRef<TO_TYPE>(instr.arg[0]) = (TO_TYPE)_getRef<FROM_TYPE>(instr.arg[2]); \
        break;                                                                      \
    }

#define CAST(TO_TYPE) NUMERIC_ITERATE(_CAST, TO_TYPE)

            // TODO Figure out a way to compress CAST with NUMERIC_ITERATE

            CAST(i8)
            CAST(u8)
            CAST(i16)
            CAST(u16)
            CAST(i32)
            CAST(u32)
            CAST(i64)
            CAST(u64)
            CAST(f32)
            CAST(f64)

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

            nk_assert(nkval_sizeof(dst) == nkval_sizeof(src));

            std::memcpy(nkval_data(dst), nkval_data(src), nkval_sizeof(dst));
            break;
        }

        case nkop_mov_8: {
            _getRef<u8>(instr.arg[0]) = _getRef<u8>(instr.arg[1]);
            break;
        }

        case nkop_mov_16: {
            _getRef<u16>(instr.arg[0]) = _getRef<u16>(instr.arg[1]);
            break;
        }

        case nkop_mov_32: {
            _getRef<u32>(instr.arg[0]) = _getRef<u32>(instr.arg[1]);
            break;
        }

        case nkop_mov_64: {
            _getRef<u64>(instr.arg[0]) = _getRef<u64>(instr.arg[1]);
            break;
        }

        case nkop_lea: {
            _getRef<void *>(instr.arg[0]) = &_getRef<u8>(instr.arg[1]);
            break;
        }

#define NUM_UN_OP_IT(TYPE, VALUE_TYPE, NAME, OP)                      \
    case NK_CAT(NK_CAT(NK_CAT(nkop_, NAME), _), TYPE): {              \
        _getRef<TYPE>(instr.arg[0]) = OP _getRef<TYPE>(instr.arg[1]); \
        break;                                                        \
    }

#define NUM_UN_OP(NAME, OP) NUMERIC_ITERATE(NUM_UN_OP_IT, NAME, OP)
#define NUM_UN_OP_INT(NAME, OP) NUMERIC_ITERATE_INT(NUM_UN_OP_IT, NAME, OP)

            NUM_UN_OP(not, not)
            NUM_UN_OP_INT(compl, compl)
            NUM_UN_OP(neg, -)

#undef NUM_UN_OP
#undef NUM_UN_OP_INT
#undef NUM_UN_OP_IT

        case nkop_eq: {
            auto dst = _getValRef(instr.arg[0]);
            auto lhs = _getValRef(instr.arg[1]);
            auto rhs = _getValRef(instr.arg[2]);

            nk_assert(nkval_sizeof(dst) == 1);
            nk_assert(nkval_sizeof(lhs) == nkval_sizeof(rhs));

            _getRef<u8>(instr.arg[0]) = std::memcmp(nkval_data(dst), nkval_data(lhs), nkval_sizeof(rhs)) == 0;
            break;
        }

        case nkop_eq_8: {
            _getRef<u8>(instr.arg[0]) = _getRef<u8>(instr.arg[1]) == _getRef<u8>(instr.arg[2]);
            break;
        }

        case nkop_eq_16: {
            _getRef<u8>(instr.arg[0]) = _getRef<u16>(instr.arg[1]) == _getRef<u16>(instr.arg[2]);
            break;
        }

        case nkop_eq_32: {
            _getRef<u8>(instr.arg[0]) = _getRef<u32>(instr.arg[1]) == _getRef<u32>(instr.arg[2]);
            break;
        }

        case nkop_eq_64: {
            _getRef<u8>(instr.arg[0]) = _getRef<u64>(instr.arg[1]) == _getRef<u64>(instr.arg[2]);
            break;
        }

        case nkop_ne: {
            auto dst = _getValRef(instr.arg[0]);
            auto lhs = _getValRef(instr.arg[1]);
            auto rhs = _getValRef(instr.arg[2]);

            nk_assert(nkval_sizeof(dst) == 1);
            nk_assert(nkval_sizeof(lhs) == nkval_sizeof(rhs));

            _getRef<u8>(instr.arg[0]) = std::memcmp(nkval_data(dst), nkval_data(lhs), nkval_sizeof(rhs)) != 0;
            break;
        }

        case nkop_ne_8: {
            _getRef<u8>(instr.arg[0]) = _getRef<u8>(instr.arg[1]) != _getRef<u8>(instr.arg[2]);
            break;
        }

        case nkop_ne_16: {
            _getRef<u8>(instr.arg[0]) = _getRef<u16>(instr.arg[1]) != _getRef<u16>(instr.arg[2]);
            break;
        }

        case nkop_ne_32: {
            _getRef<u8>(instr.arg[0]) = _getRef<u32>(instr.arg[1]) != _getRef<u32>(instr.arg[2]);
            break;
        }

        case nkop_ne_64: {
            _getRef<u8>(instr.arg[0]) = _getRef<u64>(instr.arg[1]) != _getRef<u64>(instr.arg[2]);
            break;
        }

#define NUM_BIN_OP_IT(TYPE, VALUE_TYPE, NAME, OP)                                                 \
    case NK_CAT(NK_CAT(NK_CAT(nkop_, NAME), _), TYPE): {                                          \
        _getRef<TYPE>(instr.arg[0]) = _getRef<TYPE>(instr.arg[1]) OP _getRef<TYPE>(instr.arg[2]); \
        break;                                                                                    \
    }

#define NUM_BIN_BOOL_OP_IT(TYPE, VALUE_TYPE, NAME, OP)                                          \
    case NK_CAT(NK_CAT(NK_CAT(nkop_, NAME), _), TYPE): {                                        \
        _getRef<u8>(instr.arg[0]) = _getRef<TYPE>(instr.arg[1]) OP _getRef<TYPE>(instr.arg[2]); \
        break;                                                                                  \
    }

#define NUM_BIN_OP(NAME, OP) NUMERIC_ITERATE(NUM_BIN_OP_IT, NAME, OP)
#define NUM_BIN_BOOL_OP(NAME, OP) NUMERIC_ITERATE(NUM_BIN_BOOL_OP_IT, NAME, OP)
#define NUM_BIN_OP_INT(NAME, OP) NUMERIC_ITERATE_INT(NUM_BIN_OP_IT, NAME, OP)

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
#undef NUM_BIN_OP_IT
#undef NUM_BIN_BOOL_OP
#undef NUM_BIN_BOOL_OP_IT

        default:
            nk_assert(!"unknown opcode");
            break;
    }
}

} // namespace

void nk_interp_invoke(NkBcFunct fn, nkval_t ret, nkval_t args) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    NK_LOG_DBG("program @%p", (void *)fn->prog);

    if (!ctx.is_initialized) {
        NK_LOG_TRC("initializing stack...");
        // TODO ctx.stack.reserve(1024);
        ctx.is_initialized = true;
    }

    ProgramFrame pfr{
        .base_reg = ctx.base.reg,
        .pinstr = ctx.pinstr,
    };

    ctx.base.reg = (u8 *)&ctx.reg;
    ctx.pinstr = nullptr;

    NK_LOG_DBG("instr=%p", (void *)ctx.base.instr);

    _jumpCall(fn, ret, args);

    while (ctx.pinstr) {
        auto pinstr = ctx.pinstr++;
        nk_assert(pinstr->code < nkop_count && "unknown instruction");
        NK_LOG_DBG(
            "instr: %zu %s", (pinstr - (NkBcInstr *)ctx.base.instr) * sizeof(NkBcInstr), s_nk_bc_names[pinstr->code]);

#ifdef ENABLE_LOGGING
        nkval_t dst_val{};
        auto const &dst = pinstr->arg[0];
        if (dst.ref_type != NkBcRef_None) {
            dst_val = _getValRef(dst);
        }
#endif // ENABLE_LOGGING

        interp(*pinstr);

#ifdef ENABLE_LOGGING
        if (dst_val.type) {
            NkStringBuilder sb{};
            defer {
                nksb_free(&sb);
            };
            nkval_inspect(dst_val, &sb);
            nksb_printf(&sb, ":");
            nkt_inspect(dst.type, &sb);
            NK_LOG_DBG("res=" NKS_FMT, NKS_ARG(sb));
        }
#endif // ENABLE_LOGGING
    }

    NK_LOG_TRC("exiting...");

    ctx.base.reg = pfr.base_reg;
    ctx.pinstr = pfr.pinstr;
}
