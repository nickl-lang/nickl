#include "interp.h"

#include <cstring>
#include <vector>

#include "ffi_adapter.h"
#include "ntk/allocator.h"
#include "ntk/log.h"
#include "ntk/os/syscall.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(interp);

struct ProgramFrame {
    NkBcInstr const *pinstr;
    NkFfiContext *ffi_ctx;
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
    struct Base { // repeats the layout of NkBcRefKind
        u8 *none;
        u8 *frame;
        u8 *arg;
        u8 *ret;
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
    NkFfiContext *ffi_ctx;

    ~InterpContext() {
        NK_LOG_TRC("deinitializing stack...");
        nk_assert(stack.size == 0 && "nonempty stack at exit");
        nk_arena_free(&stack);
    }
};

thread_local InterpContext ctx;

void *getRefAddr(NkBcRef const &ref) {
    return nkbc_deref(ctx.base_ar[ref.kind], &ref);
}

template <class T>
T &deref(NkBcArg const &arg) {
    nk_assert(arg.kind == NkBcArg_Ref);
    return *(T *)getRefAddr(arg.ref);
}

template <class T>
T &deref(NkBcRef const &ref) {
    return *(T *)getRefAddr(ref);
}

void jumpTo(NkBcInstr const *pinstr) {
    NK_LOG_DBG("jumping to instr@%p", (void *)pinstr);
    ctx.pinstr = pinstr;
}

void jumpTo(NkBcArg const &arg) {
    jumpTo(&deref<NkBcInstr>(arg));
}

void jumpCall(NkBcProc proc, void *const *args, void *const *ret, NkArenaFrame stack_frame) {
    ctx.ctrl_stack.emplace_back(ControlFrame{
        .stack_frame = ctx.stack_frame,
        .base_frame = ctx.base.frame,
        .base_arg = ctx.base.arg,
        .base_ret = ctx.base.ret,
        .base_instr = ctx.base.instr,
        .pinstr = ctx.pinstr,
    });

    ctx.stack_frame = stack_frame;
    ctx.base.frame = (u8 *)nk_arena_allocAligned(&ctx.stack, proc->frame_size, proc->frame_align);
    std::memset(ctx.base.frame, 0, proc->frame_size);
    ctx.base.arg = (u8 *)args;
    ctx.base.ret = (u8 *)ret;
    ctx.base.instr = (u8 *)proc->instrs.data;

    jumpTo(proc->instrs.data);

    NK_LOG_DBG("stack_frame=%" PRIu64, ctx.stack_frame.size);
    NK_LOG_DBG("frame=%p", (void *)ctx.base.frame);
    NK_LOG_DBG("arg=%p", (void *)ctx.base.arg);
    NK_LOG_DBG("ret=%p", (void *)ctx.base.ret);
    NK_LOG_DBG("pinstr=%p", (void *)ctx.pinstr);
}

void interp(NkBcInstr const &instr) {
#ifdef ENABLE_PROFILING
    NKSB_FIXED_BUFFER(sb, 128);
    nksb_printf(&sb, "interp: %s", nkbcOpcodeName(instr.code));
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

        jumpTo(fr.pinstr);
        break;
    }

    case nkop_jmp: {
        jumpTo(instr.arg[1]);
        break;
    }

    case nkop_jmpz_8: {
        if (!deref<u8>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpz_16: {
        if (!deref<u16>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpz_32: {
        if (!deref<u32>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpz_64: {
        if (!deref<u64>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpnz_8: {
        if (deref<u8>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpnz_16: {
        if (deref<u16>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpnz_32: {
        if (deref<u32>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpnz_64: {
        if (deref<u64>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_call_jmp: {
        auto const stack_frame = nk_arena_grab(&ctx.stack);

        auto const proc = deref<NkBcProc>(instr.arg[1]);

        auto const argc = instr.arg[2].refs.size;
        auto const argv = nk_arena_allocT<void *>(&ctx.stack, argc + 1);
        for (usize i = 0; i < argc; i++) {
            argv[i] = getRefAddr(instr.arg[2].refs.data[i]);
        }

        auto const retv = argv + argc;
        retv[0] = getRefAddr(instr.arg[0].ref);

        jumpCall(proc, argv, retv, stack_frame);

        break;
    }

    case nkop_call_ext: {
        auto const frame = nk_arena_grab(&ctx.stack);
        defer {
            nk_arena_popFrame(&ctx.stack, frame);
        };

        auto const argc = instr.arg[2].refs.size;
        auto const argv = nk_arena_allocT<void *>(&ctx.stack, argc);
        auto const argt = nk_arena_allocT<nktype_t>(&ctx.stack, argc);
        for (usize i = 0; i < argc; i++) {
            argv[i] = getRefAddr(instr.arg[2].refs.data[i]);
            argt[i] = instr.arg[2].refs.data[i].type;
        }

        auto const &proc_info = &instr.arg[1].ref.type->as.proc.info;

        NkNativeCallData const call_data{
            .proc{.native = deref<void *>(instr.arg[1])},
            .nfixedargs = proc_info->args_t.size,
            .is_variadic = (bool)(proc_info->flags & NkProcVariadic),
            .argv = argv,
            .argt = argt,
            .argc = argc,
            .retv = getRefAddr(instr.arg[0].ref),
            .rett = instr.arg[0].ref.type,
        };

        nk_native_invoke(ctx.ffi_ctx, &ctx.stack, &call_data);
        break;
    }

    case nkop_sext_8_16: {
        deref<i16>(instr.arg[0]) = deref<i8>(instr.arg[1]);
        break;
    }
    case nkop_sext_8_32: {
        deref<i32>(instr.arg[0]) = deref<i8>(instr.arg[1]);
        break;
    }
    case nkop_sext_8_64: {
        deref<i64>(instr.arg[0]) = deref<i8>(instr.arg[1]);
        break;
    }
    case nkop_sext_16_32: {
        deref<i32>(instr.arg[0]) = deref<i16>(instr.arg[1]);
        break;
    }
    case nkop_sext_16_64: {
        deref<i64>(instr.arg[0]) = deref<i16>(instr.arg[1]);
        break;
    }
    case nkop_sext_32_64: {
        deref<i64>(instr.arg[0]) = deref<i32>(instr.arg[1]);
        break;
    }

    case nkop_zext_8_16: {
        deref<u16>(instr.arg[0]) = deref<u8>(instr.arg[1]);
        break;
    }
    case nkop_zext_8_32: {
        deref<u32>(instr.arg[0]) = deref<u8>(instr.arg[1]);
        break;
    }
    case nkop_zext_8_64: {
        deref<u64>(instr.arg[0]) = deref<u8>(instr.arg[1]);
        break;
    }
    case nkop_zext_16_32: {
        deref<u32>(instr.arg[0]) = deref<u16>(instr.arg[1]);
        break;
    }
    case nkop_zext_16_64: {
        deref<u64>(instr.arg[0]) = deref<u16>(instr.arg[1]);
        break;
    }
    case nkop_zext_32_64: {
        deref<u64>(instr.arg[0]) = deref<u32>(instr.arg[1]);
        break;
    }

    case nkop_fext: {
        deref<f64>(instr.arg[0]) = deref<f32>(instr.arg[1]);
        break;
    }

    case nkop_trunc_16_8: {
        deref<u8>(instr.arg[0]) = deref<u16>(instr.arg[1]);
        break;
    }
    case nkop_trunc_32_8: {
        deref<u8>(instr.arg[0]) = deref<u32>(instr.arg[1]);
        break;
    }
    case nkop_trunc_64_8: {
        deref<u8>(instr.arg[0]) = deref<u64>(instr.arg[1]);
        break;
    }
    case nkop_trunc_32_16: {
        deref<u16>(instr.arg[0]) = deref<u32>(instr.arg[1]);
        break;
    }
    case nkop_trunc_64_16: {
        deref<u16>(instr.arg[0]) = deref<u64>(instr.arg[1]);
        break;
    }
    case nkop_trunc_64_32: {
        deref<u32>(instr.arg[0]) = deref<u64>(instr.arg[1]);
        break;
    }

    case nkop_ftrunc: {
        deref<f32>(instr.arg[0]) = deref<f64>(instr.arg[1]);
        break;
    }

#define FP2I_OP_IT(TYPE, VALUE_TYPE, FTYPE, SIZ)                      \
    case NK_CAT(NK_CAT(NK_CAT(nkop_fp2i_, SIZ), _), TYPE): {          \
        deref<TYPE>(instr.arg[0]) = (TYPE)deref<FTYPE>(instr.arg[1]); \
        break;                                                        \
    }

#define I2FP_OP_IT(TYPE, VALUE_TYPE, FTYPE, SIZ)                       \
    case NK_CAT(NK_CAT(NK_CAT(nkop_i2fp_, SIZ), _), TYPE): {           \
        deref<FTYPE>(instr.arg[0]) = (FTYPE)deref<TYPE>(instr.arg[1]); \
        break;                                                         \
    }

#define FP2I_OP(FTYPE, SIZ) NKIR_NUMERIC_ITERATE_INT(FP2I_OP_IT, FTYPE, SIZ)
#define I2FP_OP(FTYPE, SIZ) NKIR_NUMERIC_ITERATE_INT(I2FP_OP_IT, FTYPE, SIZ)

        FP2I_OP(f32, 32)
        FP2I_OP(f64, 64)

        I2FP_OP(f32, 32)
        I2FP_OP(f64, 64)

#undef I2FP_OP
#undef FP2I_OP
#undef I2FP_OP_IT
#undef FP2I_OP_IT

    case nkop_mov_8: {
        deref<u8>(instr.arg[0]) = deref<u8>(instr.arg[1]);
        break;
    }

    case nkop_mov_16: {
        deref<u16>(instr.arg[0]) = deref<u16>(instr.arg[1]);
        break;
    }

    case nkop_mov_32: {
        deref<u32>(instr.arg[0]) = deref<u32>(instr.arg[1]);
        break;
    }

    case nkop_mov_64: {
        deref<u64>(instr.arg[0]) = deref<u64>(instr.arg[1]);
        break;
    }

    case nkop_lea: {
        deref<void *>(instr.arg[0]) = &deref<u8>(instr.arg[1]);
        break;
    }

#define NUM_UN_OP_IT(TYPE, VALUE_TYPE, NAME, OP)                  \
    case NK_CAT(NK_CAT(NK_CAT(nkop_, NAME), _), TYPE): {          \
        deref<TYPE>(instr.arg[0]) = OP deref<TYPE>(instr.arg[1]); \
        break;                                                    \
    }

#define NUM_UN_OP(NAME, OP) NKIR_NUMERIC_ITERATE(NUM_UN_OP_IT, NAME, OP)

        NUM_UN_OP(neg, -)

#undef NUM_UN_OP
#undef NUM_UN_OP_IT

#define NUM_BIN_OP_IT(TYPE, VALUE_TYPE, NAME, OP)                                           \
    case NK_CAT(NK_CAT(NK_CAT(nkop_, NAME), _), TYPE): {                                    \
        deref<TYPE>(instr.arg[0]) = deref<TYPE>(instr.arg[1]) OP deref<TYPE>(instr.arg[2]); \
        break;                                                                              \
    }

#define NUM_BIN_BOOL_OP_IT(TYPE, VALUE_TYPE, NAME, OP)                                    \
    case NK_CAT(NK_CAT(NK_CAT(nkop_, NAME), _), TYPE): {                                  \
        deref<u8>(instr.arg[0]) = deref<TYPE>(instr.arg[1]) OP deref<TYPE>(instr.arg[2]); \
        break;                                                                            \
    }

#define NUM_BIN_OP(NAME, OP) NKIR_NUMERIC_ITERATE(NUM_BIN_OP_IT, NAME, OP)
#define NUM_BIN_BOOL_OP(NAME, OP) NKIR_NUMERIC_ITERATE(NUM_BIN_BOOL_OP_IT, NAME, OP)
#define NUM_BIN_OP_INT(NAME, OP) NKIR_NUMERIC_ITERATE_INT(NUM_BIN_OP_IT, NAME, OP)

        NUM_BIN_OP(add, +)
        NUM_BIN_OP(sub, -)
        NUM_BIN_OP(mul, *)
        NUM_BIN_OP(div, /)
        NUM_BIN_OP_INT(mod, %)

        NUM_BIN_OP_INT(and, &)
        NUM_BIN_OP_INT(or, |)
        NUM_BIN_OP_INT(xor, ^)
        NUM_BIN_OP_INT(lsh, <<)
        NUM_BIN_OP_INT(rsh, >>)

        NUM_BIN_BOOL_OP(cmp_eq, ==)
        NUM_BIN_BOOL_OP(cmp_ne, !=)
        NUM_BIN_BOOL_OP(cmp_ge, >=)
        NUM_BIN_BOOL_OP(cmp_gt, >)
        NUM_BIN_BOOL_OP(cmp_le, <=)
        NUM_BIN_BOOL_OP(cmp_lt, <)

#undef NUM_BIN_OP
#undef NUM_BIN_OP_INT
#undef NUM_BIN_OP_IT
#undef NUM_BIN_BOOL_OP
#undef NUM_BIN_BOOL_OP_IT

#if NK_SYSCALLS_AVAILABLE
    case nkop_syscall_0: {
        deref<long>(instr.arg[0]) = nk_syscall0(deref<long>(instr.arg[1]));
        break;
    }

    case nkop_syscall_1: {
        deref<long>(instr.arg[0]) = nk_syscall1(deref<long>(instr.arg[1]), deref<long>(instr.arg[2].refs.data[0]));
        break;
    }

    case nkop_syscall_2: {
        deref<long>(instr.arg[0]) = nk_syscall2(
            deref<long>(instr.arg[1]), deref<long>(instr.arg[2].refs.data[0]), deref<long>(instr.arg[2].refs.data[1]));
        break;
    }

    case nkop_syscall_3: {
        deref<long>(instr.arg[0]) = nk_syscall3(
            deref<long>(instr.arg[1]),
            deref<long>(instr.arg[2].refs.data[0]),
            deref<long>(instr.arg[2].refs.data[1]),
            deref<long>(instr.arg[2].refs.data[2]));
        break;
    }

    case nkop_syscall_4: {
        deref<long>(instr.arg[0]) = nk_syscall4(
            deref<long>(instr.arg[1]),
            deref<long>(instr.arg[2].refs.data[0]),
            deref<long>(instr.arg[2].refs.data[1]),
            deref<long>(instr.arg[2].refs.data[2]),
            deref<long>(instr.arg[2].refs.data[3]));
        break;
    }

    case nkop_syscall_5: {
        deref<long>(instr.arg[0]) = nk_syscall5(
            deref<long>(instr.arg[1]),
            deref<long>(instr.arg[2].refs.data[0]),
            deref<long>(instr.arg[2].refs.data[1]),
            deref<long>(instr.arg[2].refs.data[2]),
            deref<long>(instr.arg[2].refs.data[3]),
            deref<long>(instr.arg[2].refs.data[4]));
        break;
    }

    case nkop_syscall_6: {
        deref<long>(instr.arg[0]) = nk_syscall6(
            deref<long>(instr.arg[1]),
            deref<long>(instr.arg[2].refs.data[0]),
            deref<long>(instr.arg[2].refs.data[1]),
            deref<long>(instr.arg[2].refs.data[2]),
            deref<long>(instr.arg[2].refs.data[3]),
            deref<long>(instr.arg[2].refs.data[4]),
            deref<long>(instr.arg[2].refs.data[5]));
        break;
    }
#endif // NK_SYSCALLS_AVAILABLE

    default:
        nk_assert(!"unknown opcode");
        break;
    }
}

} // namespace

void nkir_interp_invoke(NkBcProc proc, void **args, void **ret) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    NK_LOG_DBG("program @%p", (void *)proc->ctx);

    ProgramFrame pfr{
        .pinstr = ctx.pinstr,
        .ffi_ctx = ctx.ffi_ctx,
    };

    ctx.pinstr = nullptr;
    ctx.ffi_ctx = &proc->ctx->ffi_ctx;

    NK_LOG_DBG("instr=%p", (void *)ctx.base.instr);

    jumpCall(proc, args, ret, nk_arena_grab(&ctx.stack));

    while (ctx.pinstr) {
        auto pinstr = ctx.pinstr++;

        nk_assert(pinstr->code < NkBcOpcode_Count && "unknown instruction");
        NK_LOG_DBG("instr: %zu %s", (pinstr - (NkBcInstr *)ctx.base.instr), nkbcOpcodeName(pinstr->code));

#ifdef ENABLE_LOGGING
        void *dst_ref_data = nullptr;
        auto const &dst = pinstr->arg[0];
        if (dst.kind == NkBcArg_Ref && dst.ref.kind != NkBcRef_None) {
            dst_ref_data = getRefAddr(dst.ref);
        }
#endif // ENABLE_LOGGING

        interp(*pinstr);

#ifdef ENABLE_LOGGING
        if (dst_ref_data) {
            NKSB_FIXED_BUFFER(sb, 256);
            nkirv_inspect(dst_ref_data, dst.ref.type, nksb_getStream(&sb));
            nksb_printf(&sb, ":");
            nkirt_inspect(dst.ref.type, nksb_getStream(&sb));
            NK_LOG_DBG("res=" NKS_FMT, NKS_ARG(sb));
        }
#endif // ENABLE_LOGGING
    }

    NK_LOG_TRC("exiting...");

    ctx.pinstr = pfr.pinstr;
    ctx.ffi_ctx = pfr.ffi_ctx;
}
