#include "compiler.h"

#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/logger.h"

namespace {

NK_LOG_USE_SCOPE(compiler);

} // namespace

struct NklCompiler_T {
    NkArena arena;
};

struct NklModule_T {
    NklCompiler c;
};

NklCompiler nkl_createCompiler(NklTargetTriple target) {
    NkArena arena{};
    auto alloc = nk_arena_getAllocator(&arena);
    return new (nk_alloc_t<NklCompiler_T>(alloc)) NklCompiler_T{
        .arena = arena,
    };
}

void nkl_freeCompiler(NklCompiler c) {
    auto arena = c->arena;
    nk_arena_free(&arena);
}

NklModule nkl_createModule(NklCompiler c) {
    auto alloc = nk_arena_getAllocator(&c->arena);
    return new (nk_alloc_t<NklModule_T>(alloc)) NklModule_T{
        .c = c,
    };
}

void nkl_writeModule(NklModule m, nks filename) {
    NK_LOG_TRC("%s", __func__);
}

void nkl_compile(NklModule m, NklSource src) {
    NK_LOG_TRC("%s", __func__);
}
