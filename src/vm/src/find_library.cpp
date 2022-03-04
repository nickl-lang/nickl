#include "find_library.hpp"

#include <cstring>

#include "nk/common/arena.hpp"
#include "nk/common/file_utils.hpp"
#include "nk/common/logger.hpp"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::findLibrary)

Array<string> s_search_paths;
ArenaAllocator s_arena;

} // namespace

void findLibrary_init(FindLibraryConfig const &conf) {
    LOG_TRC(__FUNCTION__)

    s_arena.init();

    s_search_paths.init(conf.search_paths.size);
    for (auto path : conf.search_paths) {
        path.copy(s_search_paths.push(), s_arena);
    }
}

void findLibrary_deinit() {
    LOG_TRC(__FUNCTION__)

    s_search_paths.deinit();
    s_arena.deinit();
}

bool findLibrary(string name, Slice<char> &buf) {
    LOG_TRC(__FUNCTION__)

    for (auto path : s_search_paths) {
        size_t size = path.size + name.size;

        if (size > MAX_PATH) {
            return false;
        }

        path.copy({buf.data, MAX_PATH});
        name.copy({buf.data + path.size, MAX_PATH});
        buf[size] = 0;

        LOG_DBG("Checking `%s`...", buf)

        if (file_exists({buf.data, size})) {
            buf.size = size;
            return true;
        }
    }

    return false;
}

} // namespace vm
} // namespace nk
