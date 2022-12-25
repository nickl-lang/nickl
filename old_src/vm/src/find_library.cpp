#include "find_library.hpp"

#include <cstring>
#include <filesystem>

#include "nk/ds/array.hpp"
#include "nk/mem/stack_allocator.hpp"
#include "nk/str/static_string_builder.hpp"
#include "nk/utils/file_utils.hpp"
#include "nk/utils/logger.h"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::find_library);

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

bool findLibrary(string name, Slice<char> &out_str) {
    LOG_TRC(__func__);

    for (auto path : s_search_paths) {
        size_t size = path.size + name.size;

        if (size > MAX_PATH) {
            LOG_ERR("path buffer overflow");
            return false;
        }

        StaticStringBuilder{out_str} << path << name << '\0';

        LOG_DBG("checking `%s`...", out_str);

        if (std::filesystem::exists(std_str(out_str))) {
            return true;
        }
    }

    out_str.size = 0;
    LOG_DBG("library not found");
    return false;
}

} // namespace vm
} // namespace nk
