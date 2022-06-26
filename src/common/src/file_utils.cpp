#include "nk/common/file_utils.hpp"

#include <fstream>

#include "nk/common/arena.hpp"
#include "nk/common/utils.hpp"

#define BUFFER_SIZE 4096

namespace nk {

Slice<char> file_read(string path, Allocator &allocator) {
    std::ifstream file{std_str(path)};
    file.exceptions(std::ios_base::badbit);

    if (!file) {
        return {};
    }

    Arena<char> ar{};
    defer {
        ar.deinit();
    };
    ar.reserve(BUFFER_SIZE);

    char *buf;
    do {
        buf = (char *)ar.push(BUFFER_SIZE);
    } while (file.read(buf, BUFFER_SIZE));
    ar.pop(BUFFER_SIZE - file.gcount());

    return ar.copy(allocator.alloc<char>(ar.size));
}

} // namespace nk
