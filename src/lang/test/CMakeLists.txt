def_test(GROUP lang NAME compiler_ast LINK ${LIB} TARGET AST_TEST)
def_test(GROUP lang NAME compiler_src LINK ${LIB} TARGET SRC_TEST)

target_compile_definitions(${AST_TEST} PRIVATE CONFIG_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
target_compile_definitions(${SRC_TEST} PRIVATE CONFIG_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
