#include "nk/vm/vm.hpp"

#include "find_library.hpp"
#include "nk/common/arena.hpp"
#include "nk/common/id.hpp"
#include "nk/common/logger.h"
#include "nk/vm/value.hpp"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm);

thread_local ArenaAllocator s_tmp_arena;
thread_local SeqAllocator *s_old_tmp_alloc;

} // namespace

void vm_init(VmConfig const &conf) {
    vm_enterThread();

    id_init();
    types_init();
    findLibrary_init(conf.find_library);
}

void vm_deinit() {
    findLibrary_deinit();
    types_deinit();
    id_deinit();

    vm_leaveThread();
}

void vm_enterThread() {
    LOG_TRC(__FUNCTION__);

    s_tmp_arena.init();
    s_old_tmp_alloc = _mctx.tmp_allocator;
    _mctx.tmp_allocator = &s_tmp_arena;
}

void vm_leaveThread() {
    LOG_TRC(__FUNCTION__);

    s_tmp_arena.deinit();
    _mctx.tmp_allocator = s_old_tmp_alloc;
}

void vm_clearTmpArena() {
    LOG_TRC(__FUNCTION__);

    s_tmp_arena.clear();
}

} // namespace vm
} // namespace nk
