#include "so_adapter.hpp"

#include <dlfcn.h>

#include "nk/str/static_string_builder.hpp"
#include "nk/utils/logger.h"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::so_adapter);

static constexpr size_t c_max_name = 256;

} // namespace

void *openSharedObject(string path) {
    LOG_TRC(__func__);

    LOG_DBG("path=%.*s", path.size, path.data);
    void *handle = dlopen(path.data, RTLD_LAZY);
    if (!handle) {
        LOG_ERR("failed to open library path=%.*s", path.size, path.data);
    }
    LOG_DBG("handle=%p", handle);
    return handle;
}

void closeSharedObject(void *handle) {
    LOG_TRC(__func__);
    if (handle && dlclose(handle)) {
        LOG_ERR("failed to close library handle=%p", handle);
    };
}

void *resolveSym(void *handle, string sym) {
    LOG_TRC(__func__);
    LOG_DBG("sym=%.*s", sym.size, sym.data);
    ARRAY_SLICE(char, name, c_max_name);
    StaticStringBuilder{name} << sym << '\0';
    void *res = dlsym(handle, name.data);
    LOG_DBG("addr=%p", res);
    return res;
}

} // namespace vm
} // namespace nk
