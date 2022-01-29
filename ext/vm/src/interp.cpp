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
        uint8_t *base_ar[static_cast<size_t>(Ref_End - Ref_Base)];
        Base base;
    };
    bool is_initialized;
    ArenaAllocator stack;
};

thread_local InterpContext s_interp_ctx;
Array<uint8_t> s_globals;

void _initCtx() {
    auto &ctx = s_interp_ctx;

    ctx.stack.init();

    ctx.base.global = s_globals.data;
    ctx.base.rodata = s_prog->rodata.data;
    ctx.base.instr = (uint8_t *)s_prog->instrs.data;
}

void _deinitCtx() {
    auto &ctx = s_interp_ctx;

    ctx.stack.deinit();
}

void _interp(Instr const &instr) {
}

} // namespace

void interp_init(Program const *prog) {
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

    LOG_TRC("interp_invoke()");

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

    uint8_t *base_frame = nullptr;
    uint8_t const *base_arg = (uint8_t *)val_data(args);
    uint8_t *base_ret = (uint8_t *)val_data(ret);

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

    DEFER({
        ctx.base.frame = base_frame;
        ctx.base.arg = base_arg;
        ctx.base.ret = base_ret;
    });

    _interp(s_prog->instrs.data[fn_info.first_instr]);
}

} // namespace vm
} // namespace nk
