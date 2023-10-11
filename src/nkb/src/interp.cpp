#include "interp.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "bytecode.hpp"
#include "ffi_adapter.hpp"
#include "ir_impl.hpp"
#include "ntk/allocator.h"
#include "ntk/logger.h"
#include "ntk/profiler.hpp"
#include "ntk/string_builder.h"
#include "ntk/sys/syscall.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(interp);

struct ProgramFrame {
    NkBcInstr const *pinstr;
    NkFfiContext *ffi_ctx;
};

struct ControlFrame {
    NkArenaFrame stack_frame;
    uint8_t *base_frame;
    uint8_t *base_arg;
    uint8_t *base_ret;
    uint8_t *base_instr;
    NkBcInstr const *pinstr;
};

struct InterpContext {
    struct Base { // repeats the layout of NkBcRefType
        uint8_t *none;
        uint8_t *frame;
        uint8_t *arg;
        uint8_t *ret;
        uint8_t *rodata;
        uint8_t *data;
        uint8_t *instr;
    };

    union {
        uint8_t *base_ar[NkBcRef_Count];
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
        assert(stack.size == 0 && "nonempty stack at exit");
        nk_arena_free(&stack);
    }
};

thread_local InterpContext ctx;

void *getRefAddr(NkBcRef const &ref) {
    uint8_t *ptr = ctx.base_ar[ref.kind] + ref.offset;
    int indir = ref.indir;
    while (indir--) {
        ptr = *(uint8_t **)ptr;
    }
    return ptr + ref.post_offset;
}

template <class T>
T &deref(NkBcArg const &arg) {
    assert(arg.kind == NkBcArg_Ref);
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
    ctx.base.frame = (uint8_t *)nk_arena_allocAligned(&ctx.stack, proc->frame_size, alignof(max_align_t));
    std::memset(ctx.base.frame, 0, proc->frame_size);
    ctx.base.arg = (uint8_t *)args;
    ctx.base.ret = (uint8_t *)ret;
    ctx.base.instr = (uint8_t *)proc->instrs.data;

    jumpTo(proc->instrs.data);

    NK_LOG_DBG("stack_frame=%" PRIu64, ctx.stack_frame.size);
    NK_LOG_DBG("frame=%p", (void *)ctx.base.frame);
    NK_LOG_DBG("arg=%p", (void *)ctx.base.arg);
    NK_LOG_DBG("ret=%p", (void *)ctx.base.ret);
    NK_LOG_DBG("pinstr=%p", (void *)ctx.pinstr);
}

void interp(NkBcInstr const &instr) {
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
        if (!deref<uint8_t>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpz_16: {
        if (!deref<uint16_t>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpz_32: {
        if (!deref<uint32_t>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpz_64: {
        if (!deref<uint64_t>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpnz_8: {
        if (deref<uint8_t>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpnz_16: {
        if (deref<uint16_t>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpnz_32: {
        if (deref<uint32_t>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_jmpnz_64: {
        if (deref<uint64_t>(instr.arg[1])) {
            jumpTo(instr.arg[2]);
        }
        break;
    }

    case nkop_call_jmp: {
        auto const stack_frame = nk_arena_grab(&ctx.stack);

        auto const proc = deref<NkBcProc>(instr.arg[1]);

        auto const argc = instr.arg[2].refs.size;
        auto const argv = nk_arena_alloc_t<void *>(&ctx.stack, argc + 1);
        for (size_t i = 0; i < argc; i++) {
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
        auto const argv = nk_arena_alloc_t<void *>(&ctx.stack, argc);
        auto const argt = nk_arena_alloc_t<nktype_t>(&ctx.stack, argc);
        for (size_t i = 0; i < argc; i++) {
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
        deref<int16_t>(instr.arg[0]) = deref<int8_t>(instr.arg[1]);
        break;
    }
    case nkop_sext_8_32: {
        deref<int32_t>(instr.arg[0]) = deref<int8_t>(instr.arg[1]);
        break;
    }
    case nkop_sext_8_64: {
        deref<int64_t>(instr.arg[0]) = deref<int8_t>(instr.arg[1]);
        break;
    }
    case nkop_sext_16_32: {
        deref<int32_t>(instr.arg[0]) = deref<int16_t>(instr.arg[1]);
        break;
    }
    case nkop_sext_16_64: {
        deref<int64_t>(instr.arg[0]) = deref<int16_t>(instr.arg[1]);
        break;
    }
    case nkop_sext_32_64: {
        deref<int64_t>(instr.arg[0]) = deref<int32_t>(instr.arg[1]);
        break;
    }

    case nkop_zext_8_16: {
        deref<uint16_t>(instr.arg[0]) = deref<uint8_t>(instr.arg[1]);
        break;
    }
    case nkop_zext_8_32: {
        deref<uint32_t>(instr.arg[0]) = deref<uint8_t>(instr.arg[1]);
        break;
    }
    case nkop_zext_8_64: {
        deref<uint64_t>(instr.arg[0]) = deref<uint8_t>(instr.arg[1]);
        break;
    }
    case nkop_zext_16_32: {
        deref<uint32_t>(instr.arg[0]) = deref<uint16_t>(instr.arg[1]);
        break;
    }
    case nkop_zext_16_64: {
        deref<uint64_t>(instr.arg[0]) = deref<uint16_t>(instr.arg[1]);
        break;
    }
    case nkop_zext_32_64: {
        deref<uint64_t>(instr.arg[0]) = deref<uint32_t>(instr.arg[1]);
        break;
    }

    case nkop_fext: {
        deref<double>(instr.arg[0]) = deref<float>(instr.arg[1]);
        break;
    }

    case nkop_trunc_16_8: {
        deref<uint8_t>(instr.arg[0]) = deref<uint16_t>(instr.arg[1]);
        break;
    }
    case nkop_trunc_32_8: {
        deref<uint8_t>(instr.arg[0]) = deref<uint32_t>(instr.arg[1]);
        break;
    }
    case nkop_trunc_64_8: {
        deref<uint8_t>(instr.arg[0]) = deref<uint64_t>(instr.arg[1]);
        break;
    }
    case nkop_trunc_32_16: {
        deref<uint16_t>(instr.arg[0]) = deref<uint32_t>(instr.arg[1]);
        break;
    }
    case nkop_trunc_64_16: {
        deref<uint16_t>(instr.arg[0]) = deref<uint64_t>(instr.arg[1]);
        break;
    }
    case nkop_trunc_64_32: {
        deref<uint32_t>(instr.arg[0]) = deref<uint64_t>(instr.arg[1]);
        break;
    }

    case nkop_ftrunc: {
        deref<float>(instr.arg[0]) = deref<double>(instr.arg[1]);
        break;
    }

#define FP2I_OP_IT(EXT, VALUE_TYPE, CTYPE, TYPE, SIZ)                  \
    case CAT(CAT(CAT(nkop_fp2i_, SIZ), _), EXT): {                     \
        deref<CTYPE>(instr.arg[0]) = (CTYPE)deref<TYPE>(instr.arg[1]); \
        break;                                                         \
    }

#define I2FP_OP_IT(EXT, VALUE_TYPE, CTYPE, TYPE, SIZ)                 \
    case CAT(CAT(CAT(nkop_i2fp_, SIZ), _), EXT): {                    \
        deref<TYPE>(instr.arg[0]) = (TYPE)deref<CTYPE>(instr.arg[1]); \
        break;                                                        \
    }

#define FP2I_OP(TYPE, SIZ) NKIR_NUMERIC_ITERATE_INT(FP2I_OP_IT, TYPE, SIZ)
#define I2FP_OP(TYPE, SIZ) NKIR_NUMERIC_ITERATE_INT(I2FP_OP_IT, TYPE, SIZ)

        FP2I_OP(float, 32)
        FP2I_OP(double, 64)

        I2FP_OP(float, 32)
        I2FP_OP(double, 64)

#undef I2FP_OP
#undef FP2I_OP
#undef I2FP_OP_IT
#undef FP2I_OP_IT

    case nkop_mov_8: {
        deref<uint8_t>(instr.arg[0]) = deref<uint8_t>(instr.arg[1]);
        break;
    }

    case nkop_mov_16: {
        deref<uint16_t>(instr.arg[0]) = deref<uint16_t>(instr.arg[1]);
        break;
    }

    case nkop_mov_32: {
        deref<uint32_t>(instr.arg[0]) = deref<uint32_t>(instr.arg[1]);
        break;
    }

    case nkop_mov_64: {
        deref<uint64_t>(instr.arg[0]) = deref<uint64_t>(instr.arg[1]);
        break;
    }

    case nkop_lea: {
        deref<void *>(instr.arg[0]) = &deref<uint8_t>(instr.arg[1]);
        break;
    }

#define NUM_UN_OP_IT(EXT, VALUE_TYPE, CTYPE, NAME, OP)              \
    case CAT(CAT(CAT(nkop_, NAME), _), EXT): {                      \
        deref<CTYPE>(instr.arg[0]) = OP deref<CTYPE>(instr.arg[1]); \
        break;                                                      \
    }

#define NUM_UN_OP(NAME, OP) NKIR_NUMERIC_ITERATE(NUM_UN_OP_IT, NAME, OP)

        NUM_UN_OP(neg, -)

#undef NUM_UN_OP
#undef NUM_UN_OP_IT

#define NUM_BIN_OP_IT(EXT, VALUE_TYPE, CTYPE, NAME, OP)                                        \
    case CAT(CAT(CAT(nkop_, NAME), _), EXT): {                                                 \
        deref<CTYPE>(instr.arg[0]) = deref<CTYPE>(instr.arg[1]) OP deref<CTYPE>(instr.arg[2]); \
        break;                                                                                 \
    }

#define NUM_BIN_BOOL_OP_IT(EXT, VALUE_TYPE, CTYPE, NAME, OP)                                     \
    case CAT(CAT(CAT(nkop_, NAME), _), EXT): {                                                   \
        deref<uint8_t>(instr.arg[0]) = deref<CTYPE>(instr.arg[1]) OP deref<CTYPE>(instr.arg[2]); \
        break;                                                                                   \
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
        deref<nksc_t>(instr.arg[0]) = nk_syscall0(deref<nksc_t>(instr.arg[1]));
        break;
    }

    case nkop_syscall_1: {
        deref<nksc_t>(instr.arg[0]) =
            nk_syscall1(deref<nksc_t>(instr.arg[1]), deref<nksc_t>(instr.arg[2].refs.data[0]));
        break;
    }

    case nkop_syscall_2: {
        deref<nksc_t>(instr.arg[0]) = nk_syscall2(
            deref<nksc_t>(instr.arg[1]),
            deref<nksc_t>(instr.arg[2].refs.data[0]),
            deref<nksc_t>(instr.arg[2].refs.data[1]));
        break;
    }

    case nkop_syscall_3: {
        deref<nksc_t>(instr.arg[0]) = nk_syscall3(
            deref<nksc_t>(instr.arg[1]),
            deref<nksc_t>(instr.arg[2].refs.data[0]),
            deref<nksc_t>(instr.arg[2].refs.data[1]),
            deref<nksc_t>(instr.arg[2].refs.data[2]));
        break;
    }

    case nkop_syscall_4: {
        deref<nksc_t>(instr.arg[0]) = nk_syscall4(
            deref<nksc_t>(instr.arg[1]),
            deref<nksc_t>(instr.arg[2].refs.data[0]),
            deref<nksc_t>(instr.arg[2].refs.data[1]),
            deref<nksc_t>(instr.arg[2].refs.data[2]),
            deref<nksc_t>(instr.arg[2].refs.data[3]));
        break;
    }

    case nkop_syscall_5: {
        deref<nksc_t>(instr.arg[0]) = nk_syscall5(
            deref<nksc_t>(instr.arg[1]),
            deref<nksc_t>(instr.arg[2].refs.data[0]),
            deref<nksc_t>(instr.arg[2].refs.data[1]),
            deref<nksc_t>(instr.arg[2].refs.data[2]),
            deref<nksc_t>(instr.arg[2].refs.data[3]),
            deref<nksc_t>(instr.arg[2].refs.data[4]));
        break;
    }

    case nkop_syscall_6: {
        deref<nksc_t>(instr.arg[0]) = nk_syscall6(
            deref<nksc_t>(instr.arg[1]),
            deref<nksc_t>(instr.arg[2].refs.data[0]),
            deref<nksc_t>(instr.arg[2].refs.data[1]),
            deref<nksc_t>(instr.arg[2].refs.data[2]),
            deref<nksc_t>(instr.arg[2].refs.data[3]),
            deref<nksc_t>(instr.arg[2].refs.data[4]),
            deref<nksc_t>(instr.arg[2].refs.data[5]));
        break;
    }
#endif // NK_SYSCALLS_AVAILABLE

    default:
        assert(!"unknown opcode");
        break;
    }
}

} // namespace

void nkir_interp_invoke(NkBcProc proc, void **args, void **ret) {
    EASY_FUNCTION(::profiler::colors::Red200);

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

        assert(pinstr->code < NkBcOpcode_Count && "unknown instruction");
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
            nksb_fixed_buffer(sb, 256);
            nkirv_inspect(dst_ref_data, dst.ref.type, &sb);
            nksb_printf(&sb, ":");
            nkirt_inspect(dst.ref.type, &sb);
            NK_LOG_DBG("res=" nks_Fmt, nks_Arg(sb));
        }
#endif // ENABLE_LOGGING
    }

    NK_LOG_TRC("exiting...");

    ctx.pinstr = pfr.pinstr;
    ctx.ffi_ctx = pfr.ffi_ctx;
}
