#include "nkl/core/core.hpp"

#include <iostream>
#include <sstream>

#include "nk/common/file_utils.hpp"
#include "nk/common/logger.hpp"
#include "nk/vm/bc.hpp"
#include "nk/vm/c_compiler_adapter.hpp"
#include "nk/vm/interp.hpp"
#include "nk/vm/vm.hpp"
#include "nkl/core/compiler.hpp"
#include "nkl/core/lexer.hpp"
#include "nkl/core/parser.hpp"

namespace nkl {

namespace {

using namespace nk;

LOG_USE_SCOPE(nkl::core)

struct FileCtx {
    string path;
};

Array<FileCtx> s_file_stack;

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

void __compile_intrinsic(type_t, value_t ret, value_t args) {
    auto file_val = vm::val_tuple_at(args, 0);
    auto file = val_as(char const *, file_val);
    size_t const file_len = vm::val_array_size(vm::val_ptr_deref(file_val));

    auto outfile_val = vm::val_tuple_at(args, 1);
    auto outfile = val_as(char const *, outfile_val);
    size_t const outfile_len = vm::val_array_size(vm::val_ptr_deref(outfile_val));

    val_as(int8_t, ret) = lang_compileFile(string{file, file_len}, string{outfile, outfile_len});
}

void _setupInstrinsics(Compiler &c) {
    using namespace vm;

#define INTRINSIC(NAME, RET_T)               \
    c.intrinsics.insert(cs2id("__" #NAME)) = \
        type_get_fn(RET_T, type_get_tuple({}), 0, __##NAME##_intrinsic, nullptr)

    INTRINSIC(printf, type_get_numeric(Uint64));
    INTRINSIC(assert, type_get_void());
    INTRINSIC(typeof, type_get_typeref());
    INTRINSIC(compile, type_get_numeric(Int8));

#undef INTRINSIC
}

bool _compileFile(string file, Compiler &compiler) {
    string path{};
    if (path_isAbsolute(file)) {
        path = file;
    } else {
        path = path_concat(
            s_file_stack.size == 0 ? path_current() : path_parent(s_file_stack.back().path), file);
    }

    s_file_stack.push() = {
        .path = path,
    };

    auto src = file_read(path);
    defer {
        src.deinit();
    };

    if (!src.data) {
        std::cerr << "error: failed to read file `" << path << "`" << std::endl;
        return false;
    }

    Lexer lexer{};
    defer {
        lexer.tokens.deinit();
    };

    if (!lexer.lex(src.slice())) {
        std::cerr << "error: " << lexer.err << std::endl;
        return false;
    }

    Parser parser{};
    defer {
        parser.ast.deinit();
    };

    if (!parser.parse(lexer.tokens.slice())) {
        std::cerr << "error: " << parser.err << std::endl;
        return false;
    }

    if (!compiler.compile(parser.root)) {
        std::cerr << "error: " << compiler.err << std::endl;
        return false;
    }

    return true;
}

} // namespace

void lang_init() {
    LOG_TRC("lang_init")

    vm::VmConfig conf;
    string paths[] = {cs2s("/usr/lib/"), cs2s("/usr/lib/x86_64-linux-gnu/")};
    conf.find_library.search_paths = {paths, sizeof(paths) / sizeof(paths[0])};
    vm_init(conf);

    s_file_stack.init(1);
}

void lang_deinit() {
    LOG_TRC("lang_deinit")

    s_file_stack.deinit();

    vm::vm_deinit();
}

int lang_runFile(string file) {
    LOG_TRC("lang_runFile(file=%s)", file)

    Compiler compiler{};
    defer {
        compiler.deinit();
    };

    _setupInstrinsics(compiler);

    if (!_compileFile(file, compiler)) {
        return 1;
    }

    vm::bc::Program prog{};
    defer {
        prog.deinit();
    };

    vm::bc::translateFromIr(prog, compiler.prog);

    auto top_fn = prog.funct_info[0].funct_t;
    vm::val_fn_invoke(top_fn, {}, {});

    return 0;
}

bool lang_compileFile(string file, string outfile) {
    LOG_TRC("lang_compileFile(file=%s, outfile=%s)", file, outfile)

    Compiler compiler{};
    defer {
        compiler.deinit();
    };

    _setupInstrinsics(compiler);

    if (!_compileFile(file, compiler)) {
        return false;
    }

    vm::CCompilerConfig conf = {
        .compiler_binary = cs2s("gcc"),
        .additional_flags = cs2s("-O2"),
        .output_filename = outfile,
        .quiet = true,
    };

    if (!vm::c_compiler_compile(conf, compiler.prog)) {
        std::cerr << "error: c compiler failed" << std::endl;
        return false;
    }

    return false;
}

} // namespace nkl
