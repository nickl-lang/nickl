#include "nkl/lang/lang.hpp"

#include <iostream>

// #include "nk/mem/stack_allocator.hpp"
#include "nk/str/static_string_builder.hpp"
// #include "nk/utils/file_utils.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
#include "nkl/lang/compiler.hpp"
// #include "nkl/lang/lexer.hpp"
// #include "nkl/lang/parser.hpp"
// #include "nkl/lang/value.hpp"

namespace nkl {

LOG_USE_SCOPE(nkl);

void lang_init() {
    EASY_FUNCTION();
    LOG_TRC(__func__);
}

void lang_deinit() {
    EASY_FUNCTION();
    LOG_TRC(__func__);
}

int lang_runFile(string filename) {
    EASY_FUNCTION();
    LOG_TRC(__func__);

    //@Tmp Commented out stuff in lang_runFile

    ARRAY_SLICE(char, err_msg, 1024);
    StaticStringBuilder err_sb{err_msg};

    // StackAllocator arena{};
    // defer {
    //     arena.deinit();
    // };

    ////@Todo Compiler will probably be responsible for reading files...
    // auto src = file_read(filename, arena);

    // if (!src.data) {
    //     std::cerr << "error: failed to read file `" << filename << "`" << std::endl;
    //     return 1;
    // }

    // id_init();
    // types::init();
    // defer {
    //     types::deinit();
    //     id_deinit();
    // };

    // Lexer lexer{err_sb};
    // defer {
    //     lexer.tokens.deinit();
    // };

    // if (!lexer.lex(src)) {
    //     std::cerr << "error: " << err_sb.moveStr() << std::endl;
    //     return 1;
    // }

    // Parser parser{err_sb};
    // defer {
    //     parser.ast.deinit();
    // };

    // if (!parser.parse(lexer.tokens)) {
    //     std::cerr << "error: " << err_sb.moveStr() << std::endl;
    //     return 1;
    // }

    Compiler compiler{err_sb};

    if (!compiler.runFile(filename)) {
        std::cerr << "error: " << err_sb.moveStr() << std::endl;
        return 1;
    }

    return 0;
}

} // namespace nkl
