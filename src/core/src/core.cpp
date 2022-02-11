#include "nkl/core/core.hpp"

#include <iostream>

#include "nk/common/file_utils.hpp"
#include "nk/common/logger.hpp"
#include "nk/vm/vm.hpp"
#include "nkl/core/lexer.hpp"
#include "nkl/core/parser.hpp"

namespace nkl {

namespace {

using namespace nk;

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

    vm::vm_init({});
    DEFER({ vm::vm_deinit(); })

    auto src = read_file(filename);
    DEFER({ src.deinit(); })

    if (!src.data) {
        fprintf(stderr, "error: failed to read file `%s`\n", filename);
        return 1;
    }

    Lexer lexer{};
    DEFER({ lexer.tokens.deinit(); })

    if (!lexer.lex(src.slice())) {
        std::cerr << "error: " << lexer.err << std::endl;
        return 1;
    }

    Parser parser{};
    DEFER({ parser.ast.deinit(); })

    if (!parser.parse(lexer.tokens.slice())) {
        std::cerr << "error: " << parser.err << std::endl;
        return 1;
    }

    return 0;
}

} // namespace nkl
