#include "nkl/core/core.hpp"

#include <iostream>
#include <sstream>

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

void __printf_intrinsic(type_t, value_t ret, value_t args) {
    auto fmt_val = vm::val_tuple_at(args, 0);
    auto fmt = val_as(char const *, fmt_val);
    size_t const fmt_len = vm::val_array_size(vm::val_ptr_deref(fmt_val));

    std::ostringstream ss;

    size_t argi = 1;
    size_t start = 0;
    for (size_t i = 0; i < fmt_len; i++) {
        if (i < fmt_len - 1 && fmt[i] == '{' && fmt[i + 1] == '}') {
            ss << string{fmt + start, i - start};
            ss << vm::val_inspect(vm::val_tuple_at(args, argi++));
            i += 2;
            start = i;
        }
    }
    ss << string{fmt + start, fmt_len - start};

    auto const &str = ss.str();
    val_as(uint64_t, ret) = str.size();
    std::cout << str;
}

void __assert_intrinsic(type_t, value_t, value_t args) {
    vm::val_numeric_visit(vm::val_tuple_at(args, 0), [](auto val) {
        if (!val) {
            std::cout << "Assertion failed!" << std::endl;
            std::abort();
        }
    });
}

void __typeof_intrinsic(type_t, value_t ret, value_t args) {
    val_as(type_t, ret) = vm::val_typeof(vm::val_tuple_at(args, 0));
}

void _setupInstrinsics(Compiler &c) {
    using namespace vm;

#define INTRINSIC(NAME, RET_T)               \
    c.intrinsics.insert(cs2id("__" #NAME)) = \
        type_get_fn(RET_T, type_get_tuple({}), 0, __##NAME##_intrinsic, nullptr)

    INTRINSIC(printf, type_get_numeric(Uint64));
    INTRINSIC(assert, type_get_void());
    INTRINSIC(typeof, type_get_typeref());

#undef INTRINSIC
}

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

    Compiler compiler{};
    DEFER({ compiler.deinit(); })

    _setupInstrinsics(compiler);

    if (!compiler.compile(parser.root)) {
        std::cerr << "error: " << compiler.err << std::endl;
        return 1;
    }

    vm::bc::Program prog{};
    DEFER({ prog.deinit(); })

    vm::bc::translateFromIr(prog, compiler.prog);

    auto top_fn = prog.funct_info[0].funct_t;
    vm::val_fn_invoke(top_fn, {}, {});

    return 0;
}

} // namespace nkl
