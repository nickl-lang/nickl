#include "nkl/lang/compiler.h"

#include <new>

#include "nk/common/allocator.h"
#include "nk/common/logger.h"

namespace {

NK_LOG_USE_SCOPE(compiler);

} // namespace

struct NklCompiler_T {};

NklCompiler nkl_compiler_create() {
    return new (nk_allocate(nk_default_allocator, sizeof(NklCompiler_T))) NklCompiler_T{};
}

void nkl_compiler_free(NklCompiler c) {
    c->~NklCompiler_T();
    nk_free(nk_default_allocator, c);
}

void nkl_run(NklCompiler c, NklAstNode root) {
    NK_LOG_TRC(__func__);
}
