#include "nkl/core/core.hpp"

#include <iostream>

#include "nk/common/file_utils.hpp"
#include "nk/common/logger.hpp"
#include "nk/vm/bc.hpp"
#include "nk/vm/interp.hpp"
#include "nk/vm/vm.hpp"
#include "nkl/core/compiler.hpp"
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

    vm::VmConfig conf;
    string paths[] = {cs2s("/usr/lib/")};
    conf.find_library.search_paths = {paths, sizeof(paths) / sizeof(paths[0])};
    vm_init(conf);
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

    // Compiler compiler{};
    // DEFER({ compiler.prog.deinit(); })

    // if (!compiler.compile(parser.root)) {
    //     std::cerr << "error: " << compiler.err << std::endl;
    //     return 1;
    // }

    // vm::bc::Program prog{};
    // DEFER({ prog.deinit(); })

    // vm::bc::bc_translateFromIr(prog, compiler.prog);

    // auto top_fn = prog.funct_info[0].funct_t;
    // vm::val_fn_invoke(top_fn, {}, {});

    return 0;
}

} // namespace nkl
