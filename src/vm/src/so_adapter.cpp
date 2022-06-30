#include "so_adapter.hpp"

#include <dlfcn.h>

#include "nk/common/logger.h"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::so_adapter);

static constexpr size_t c_max_name = 255;

} // namespace

void *openSharedObject(string path) {
    LOG_TRC(__func__);

    LOG_DBG("path=%.*s", path.size, path.data);
    void *handle = dlopen(path.data, RTLD_LAZY);
    LOG_DBG("handle=%p", handle);
    return handle;
}

void closeSharedObject(void *handle) {
    LOG_TRC(__func__);
    if (dlclose(handle)) {
        LOG_ERR("failed to close library handle=%p", handle);
    };
}

void *resolveSym(void *handle, string sym) {
    LOG_TRC(__func__);
    LOG_DBG("sym=%.*s", sym.size, sym.data);
    char name_buf[c_max_name];
    sym.copy({name_buf, c_max_name});
    name_buf[sym.size] = 0;
    void *res = dlsym(handle, name_buf);
    LOG_DBG("addr=%p", res);
    return res;
}

} // namespace vm
} // namespace nk
