#include "nk/common/file_utils.hpp"

#include <fstream>
#include <memory>

Array<char> read_file(char const *filename) {
    static constexpr size_t c_read_size = 4096;

    std::ifstream file{filename};
    file.exceptions(std::ios_base::badbit);

    if (!file) {
        return {0, 0, nullptr};
    }

    auto ar = Array<char>::create(c_read_size);

    char *buf;
    do {
        buf = &ar.push(c_read_size);
    } while (file.read(buf, c_read_size));
    ar.pop(c_read_size - file.gcount());

    return ar;
}
