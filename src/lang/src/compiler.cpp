#include "nkl/lang/compiler.hpp"

#include "nk/ds/array.hpp"
#include "nk/ds/hashmap.hpp"
#include "nk/ds/log2arena.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
#include "nk/vm/ir.hpp"
#include "nkl/lang/compiler_api.hpp"
#include "nkl/lang/lexer.hpp"
#include "nkl/lang/parser.hpp"

#define CHECK(EXPR)         \
    EXPR;                   \
    if (m_error_occurred) { \
        return {};          \
    }

#define DEFINE(VAR, VAL) CHECK(auto VAR = (VAL))
#define ASSIGN(SLOT, VAL) CHECK(SLOT = (VAL))
#define APPEND(AR, VAL) CHECK(*AR.push() = (VAL))

namespace nkl {

namespace {

using namespace nk::vm;

struct FileRecord {
    string text;
    Lexer lexer;
    Parser parser;
};

enum EDeclType {
    Decl_Undefined,

    Decl_LocalVar,
    Decl_GlobalVar,
    Decl_Funct,
    Decl_ExtFunct,
    Decl_Arg,
    Decl_Namespace,
};

struct Namespace;

struct Decl {
    union {
        struct {
            union {
                ir::Local local_id;
                ir::Global global_id;
            };
            type_t type;
            bool is_const;
        } var;
        ir::FunctId funct_id;
        ir::ExtFunctId ext_funct_id;
        struct {
            size_t index;
            type_t type;
        } arg;
        Namespace *ns;
    } as;
    EDeclType decl_type;
};

struct Namespace {
    HashMap<Id, Decl> names;
};

struct Scope {
    Namespace ns;
    Array<NodeRef> defers;
};

struct CompilerState {
    Array<FileRecord> file_stack;
    Array<Scope> scopes;
    Log2Arena<Namespace> namespaces;
    HashMap<string, Namespace *> imports;
};

CompilerState s_state;

} // namespace

bool Compiler::runFile(string filename) {
    return true;
}

extern "C" bool nk_compiler_defineVar(string name, type_t type) {
}

extern "C" bool nk_compiler_defineVarInit(string name, value_t value) {
}

extern "C" bool nk_compiler_defineConst(string name, value_t value) {
}

} // namespace nkl
