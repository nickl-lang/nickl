#include "stc.h"

#include <filesystem>

#include "nkl/common/ast.h"
#include "nkl/common/diagnostics.h"
#include "ntk/allocator.h"
#include "ntk/file.h"
#include "ntk/logger.h"
#include "ntk/string.h"
#include "ntk/utils.h"

namespace {

namespace fs = std::filesystem;

NK_LOG_USE_SCOPE(nkstc);

} // namespace

int nkst_compile(nks in_file) {
    NK_LOG_TRC("%s", __func__);

    NkArena arena{};
    defer {
        nk_arena_free(&arena);
    };

    auto in_file_path = fs::current_path() / fs::path{std_str(in_file)};

    if (!fs::exists(in_file_path)) {
        auto const in_file_path_str = in_file_path.string();
        nkl_diag_printError("file `%s` doesn't exist", in_file_path_str.c_str());
        return false;
    }

    in_file_path = fs::canonical(in_file_path);
    auto const in_file_path_str = in_file_path.string();

    auto const in_file_s = nks{in_file_path_str.c_str(), in_file_path_str.size()};

    auto read_res = nk_file_read(nk_arena_getAllocator(&arena), in_file_s);
    if (!read_res.ok) {
        nkl_diag_printError("failed to read file `%s`", in_file_path_str.c_str());
        return false;
    }

    // TODO Hardcoded example {
    NklAstNode nodes[] = {
        {.id = cs2nkid("add"), .token_idx = 1, .total_children = 2, .arity = 2},
        {.id = cs2nkid("int"), .token_idx = 0, .total_children = 0, .arity = 0},
        {.id = cs2nkid("int"), .token_idx = 2, .total_children = 0, .arity = 0},
    };
    NklToken tokens[] = {
        {.text = nk_cs2s("4"), .id = 0, .lin = 1, .col = 1},
        {.text = nk_cs2s("+"), .id = 0, .lin = 1, .col = 2},
        {.text = nk_cs2s("5"), .id = 0, .lin = 1, .col = 3},
    };
    nk_stream out = nk_file_getStream(nk_stdout());
    nkl_ast_inspect({nodes, sizeof(nodes)}, {tokens, sizeof(tokens)}, out);
    // }

    return 0;
}
