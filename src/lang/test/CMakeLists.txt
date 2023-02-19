def_test(GROUP lang NAME compiler_ast LINK ${LIB} TARGET AST_TEST)
def_test(GROUP lang NAME compiler_src LINK ${LIB} TARGET SRC_TEST)

set(STDLIB_DIR ${PROJECT_SOURCE_DIR}/stdlib)

target_compile_definitions(${AST_TEST} PRIVATE STDLIB_DIR="${STDLIB_DIR}")
target_compile_definitions(${SRC_TEST} PRIVATE STDLIB_DIR="${STDLIB_DIR}")
