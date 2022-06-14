#include "nk/common/file_utils.hpp"

#include <filesystem>
#include <fstream>
#include <memory>

#include "nk/common/profiler.hpp"

namespace {

string _p2s(std::filesystem::path const &path) {
    return tmpstr_format("%.*s", path.native().size(), path.native().data());
}

std::filesystem::path _s2p(string path) {
    return std::filesystem::path{std_view(path)};
}

} // namespace

Array<char> file_read(string path) {
    EASY_FUNCTION(profiler::colors::Grey200)

    static constexpr size_t c_read_size = 4096;

    std::ifstream file{_s2p(path)};
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

bool file_exists(string path) {
    return std::filesystem::exists(_s2p(path));
}

string path_concat(string lhs, string rhs) {
    return _p2s(_s2p(lhs) / _s2p(rhs));
}

bool path_isAbsolute(string path) {
    return _s2p(path).is_absolute();
}

string path_current() {
    return _p2s(std::filesystem::current_path());
}

string path_parent(string path) {
    return _p2s(_s2p(path).parent_path());
}
