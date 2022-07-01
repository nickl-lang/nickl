#include "find_library.hpp"

#include <cstring>
#include <filesystem>

#include "nk/common/array.hpp"
#include "nk/common/file_utils.hpp"
#include "nk/common/logger.h"
#include "nk/common/stack_allocator.hpp"
#include "nk/common/static_string_builder.hpp"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::findLibrary);

Array<string> s_search_paths;
StackAllocator s_arena;

} // namespace

void findLibrary_init(FindLibraryConfig const &conf) {
    LOG_TRC(__func__);

    s_search_paths.reserve(conf.search_paths.size);
    for (auto path : conf.search_paths) {
        *s_search_paths.push() = path.copy(s_arena.alloc<char>(path.size));
    }
}

void findLibrary_deinit() {
    LOG_TRC(__func__);

    s_search_paths.deinit();
    s_arena.deinit();
}

bool findLibrary(string name, Slice<char> &buf) {
    LOG_TRC(__func__);

    for (auto path : s_search_paths) {
        size_t size = path.size + name.size;

        if (size > MAX_PATH) {
            LOG_ERR("path buffer overflow");
            return false;
        }

        StaticStringBuilder sb{buf};
        sb << path << name;
        auto const str = sb.moveStr();

        LOG_DBG("Checking `%s`...", str);

        if (std::filesystem::exists(std_str(str))) {
            return true;
        }
    }

    buf.size = 0;
    LOG_DBG("Library not found");
    return false;
}

} // namespace vm
} // namespace nk
