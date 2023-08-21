#include "nk/common/file.h"

#include <fstream>

#include "nk/common/allocator.hpp"
#include "nk/common/array.hpp"
#include "nk/common/string.hpp"

NkReadFileResult nk_readFile(NkAllocator alloc, nkstr path) {
    std::ifstream file{std_str(path), std::ios::in | std::ios::binary | std::ios::ate};

    if (!file) {
        return {};
    }

    size_t const file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    auto bytes = nk_alloc_t<char>(alloc, file_size + 1);
    file.read(bytes, file_size);

    return {
        .bytes = {bytes, file_size},
        .ok = true,
    };
}
