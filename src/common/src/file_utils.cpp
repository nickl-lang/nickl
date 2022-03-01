#include "nk/common/file_utils.hpp"

#include <fstream>
#include <memory>

#include "nk/common/profiler.hpp"

#define MAX_PATH 4096

Array<char> read_file(string filename) {
    EASY_FUNCTION(profiler::colors::Grey200)

    static constexpr size_t c_read_size = 4096;

    if (filename.size > MAX_PATH - 1) {
        return {};
    }

    char path_buf[MAX_PATH];
    filename.copy({path_buf, MAX_PATH - 1});
    path_buf[filename.size] = '\0';

    std::ifstream file{path_buf};
    file.exceptions(std::ios_base::badbit);

    if (!file) {
        return {};
    }

    auto ar = Array<char>::create(c_read_size);

    char *buf;
    do {
        buf = &ar.push(c_read_size);
    } while (file.read(buf, c_read_size));
    ar.pop(c_read_size - file.gcount());

    return ar;
}
