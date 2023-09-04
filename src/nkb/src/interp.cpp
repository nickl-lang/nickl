#include "interp.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>

#include "bytecode.hpp"
#include "ffi_adapter.hpp"
#include "ir_impl.hpp"
#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nk/common/string_builder.h"
#include "nk/common/utils.h"
#include "nk/common/utils.hpp"

namespace {

NK_LOG_USE_SCOPE(interp);

struct ProgramFrame {
    NkBcInstr const *pinstr;
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

    ~InterpContext() {
        NK_LOG_TRC("deinitializing stack...");
        assert(nk_arena_grab(&stack).size == 0 && "nonempty stack at exit");
        nk_arena_free(&stack);
    }
};

thread_local InterpContext ctx;

void *getRefAddr(NkBcRef const &ref) {
    auto ptr = ctx.base_ar[ref.kind] + ref.offset;
    if (ref.is_indirect) {
        ptr = *(uint8_t **)ptr;
    }
    return ptr + ref.post_offset;
}

template <class T>
T &deref(NkBcArg const &arg) {
    return *(T *)getRefAddr(arg.ref);
}

void jumpTo(NkBcInstr const *pinstr) {
    NK_LOG_DBG("jumping to instr@%p", (void *)pinstr);
    ctx.pinstr = pinstr;
}

void jumpTo(NkBcArg const &arg) {
    jumpTo(&deref<NkBcInstr>(arg));
}

void jumpCall(NkBcProc proc, void *args, void *ret) {
    ctx.ctrl_stack.emplace_back(ControlFrame{
        .stack_frame = ctx.stack_frame,
        .base_frame = ctx.base.frame,
        .base_arg = ctx.base.arg,
        .base_ret = ctx.base.ret,
        .base_instr = ctx.base.instr,
        .pinstr = ctx.pinstr,
    });

    ctx.stack_frame = nk_arena_grab(&ctx.stack);
    ctx.base.frame = (uint8_t *)nk_arena_alloc(&ctx.stack, proc->frame_size); // TODO not aligned
    std::memset(ctx.base.frame, 0, proc->frame_size);
    ctx.base.arg = (uint8_t *)args;
    ctx.base.ret = (uint8_t *)ret;
    ctx.base.instr = (uint8_t *)proc->instrs.data();

    jumpTo(proc->instrs.data());

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

    case nkop_call: {
        NK_LOG_WRN("TODO nkop_call execution not implemented");
        break;
    }

    case nkop_call_jmp: {
        NK_LOG_WRN("TODO nkop_call_jmp execution not implemented");
        break;
    }

    case nkop_call_ext: {
        auto proc = deref<void *>(instr.arg[1]);

        auto const frame = nk_arena_grab(&ctx.stack);
        defer {
            nk_arena_popFrame(&ctx.stack, frame);
        };

        size_t argc = instr.arg[2].refs.size;
        auto argv = (void **)nk_arena_alloc(&ctx.stack, argc * sizeof(void *));
        for (size_t i = 0; i < argc; i++) {
            argv[i] = getRefAddr(instr.arg[2].refs.data[i]);
        }
        void *retv = getRefAddr(instr.arg[0].ref);

        // TODO Hardcoded proc_info
        NkIrType i64{
            .as{.basic{Int64}},
            .size = 8,
            .align = 8,
            .kind = NkType_Basic,
        };
        nktype_t i64_ptr = &i64;
        NkIrType i32{
            .as{.basic{Int32}},
            .size = 4,
            .align = 4,
            .kind = NkType_Basic,
        };
        nktype_t i32_ptr = &i32;
        NkIrProcInfo proc_info{
            .args_t{&i64_ptr, 1},
            .ret_t{&i32_ptr, 1},
            .call_conv = NkCallConv_Cdecl,
            .flags = 0,
        };

        nk_native_invoke(proc, &proc_info, argv, argc, &retv);
        break;
    }

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

    default:
        assert(!"unknown opcode");
        break;
    }
}

} // namespace

void nkir_interp_invoke(NkBcProc proc, void *args, void *ret) {
    EASY_FUNCTION(::profiler::colors::Red200);

    NK_LOG_TRC("%s", __func__);

    NK_LOG_DBG("program @%p", (void *)proc->ctx);

    ProgramFrame pfr{
        .pinstr = ctx.pinstr,
    };

    ctx.pinstr = nullptr;

    NK_LOG_DBG("instr=%p", (void *)ctx.base.instr);

    jumpCall(proc, args, ret);

    while (ctx.pinstr) {
        auto pinstr = ctx.pinstr++;
        assert(pinstr->code < NkBcOpcode_Count && "unknown instruction");
        NK_LOG_DBG("instr: %zu %s", (pinstr - (NkBcInstr *)ctx.base.instr), nkbcOpcodeName(pinstr->code));
        interp(*pinstr);
        NK_LOG_DBG(
            "res=%s", (char const *)[&]() {
                NkStringBuilder sb{};
                char const *str{};
                auto const &arg = pinstr->arg[0];
                if (arg.ref.kind != NkBcRef_None) {
                    sb = nksb_create();
                    nkirv_inspect(&deref<uint8_t>(arg), arg.ref.type, sb);
                    nksb_printf(sb, ":");
                    nkirt_inspect(arg.ref.type, sb);
                    str = nksb_concat(sb).data;
                }
                return makeDeferrerWithData(str, [sb]() {
                    nksb_free(sb);
                });
            }());
    }

    NK_LOG_TRC("exiting...");

    ctx.pinstr = pfr.pinstr;
}