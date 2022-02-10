#include "nkl/core/core.hpp"

#include "nk/common/file_utils.hpp"
#include "nk/common/logger.hpp"
#include "nkl/core/lexer.hpp"

namespace nkl {

namespace {

LOG_USE_SCOPE(nkl::core)

} // namespace

void lang_init() {
    LOG_TRC("lang_init")
}

void lang_deinit() {
    LOG_TRC("lang_deinit")
}

int lang_runFile(char const *filename) {
    LOG_TRC("lang_runFile(filename=%s)", filename)

    auto src = read_file(filename);
    DEFER({ src.deinit(); });

    if (!src.data) {
        fprintf(stderr, "error: failed to read file `%s`\n", filename);
        return 1;
    }

    Lexer lexer{};
    DEFER({ lexer.tokens.deinit(); })

    lexer.lex(src.slice());

    return 0;
}

} // namespace nkl
