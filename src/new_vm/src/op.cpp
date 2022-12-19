#include "op.h"

#include <cassert>
#include <new>
#include <tuple>

#include "interp.hpp"
#include "ir_internal.hpp"
#include "nk/common/allocator.h"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"
#include "op_internal.hpp"

namespace {

FunctInfo _translateIr(NkOpProg p, NkIrFunctId fn) {
}

} // namespace

NkOpProg nkop_createProgram(NkIrProg ir) {
    return new (nk_allocate(nk_default_allocator, sizeof(NkOpProg_T))) NkOpProg_T{
        .ir = ir,
        .arena = nk_create_arena(),
    };
}

void nkop_deinitProgram(NkOpProg p) {
    if (p) {
        nk_free_arena(p->arena);
        p->~NkOpProg_T();
        nk_free(nk_default_allocator, p);
    }
}

void nkop_invoke(NkOpProg p, NkIrFunctId fn, nkval_t ret, nkval_t args) {
    //@TODO Add trace/debug logs

    assert(fn.id < p->ir->functs.size() && "invalid function");

    auto it = p->functs.find(fn.id);
    if (it == std::end(p->functs)) {
        std::tie(it, std::ignore) = p->functs.emplace(fn.id, _translateIr(p, fn));
    }

    nk_interp_invoke(it->second, ret, args);
}
