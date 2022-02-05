#include "nk/vm/so_adapter.hpp"

#include <dlfcn.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::so_adapter)

static constexpr size_t c_max_path = 4096;
static constexpr size_t c_max_name = 255;

Array<string> s_search_paths;
ArenaAllocator s_arena;

} // namespace

void so_adapter_init(Slice<string> search_paths) {
    s_arena.init();

    s_search_paths.init(search_paths.size);
    for (auto path : search_paths) {
        s_search_paths.push() = copy(path, s_arena);
    }
}

void so_adapter_deinit() {
    s_search_paths.deinit();
    s_arena.deinit();
}

void *openSharedObject(string name) {
    if (name.size >= c_max_name) {
        return nullptr;
    }
    LOG_TRC(__FUNCTION__);
    LOG_DBG("name=%.*s", name.size, name.data);
    char path_buf[c_max_path];
    void *handle = nullptr;
    for (auto path : s_search_paths) {
        if (path.size + name.size >= c_max_path) {
            continue;
        }
        char *name_buf = copy(path, path_buf);
        char *null = copy(name, name_buf);
        null[0] = 0;
        handle = dlopen(path_buf, RTLD_LAZY);
        if (handle) {
            LOG_DBG("path=%s", path_buf);
            break;
        }
    }
    LOG_DBG("handle=%p", handle);
    return handle;
}

void closeSharedObject(void *handle) {
    LOG_TRC(__FUNCTION__);
    if (dlclose(handle)) {
        LOG_ERR("failed to close library handle=%p", handle);
    };
}

void *resolveSym(void *handle, string sym) {
    LOG_TRC(__FUNCTION__);
    LOG_DBG("sym=%.*s", sym.size, sym.data);
    char name_buf[c_max_name];
    char *null = copy(sym, name_buf);
    null[0] = 0;
    void *res = dlsym(handle, name_buf);
    LOG_DBG("addr=%p", res);
    return res;
}

} // namespace vm
} // namespace nk
