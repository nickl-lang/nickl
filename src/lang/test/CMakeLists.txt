def_test(GROUP lang NAME compiler_ast LINK nkl_lang TARGET AST_TEST)
def_test(GROUP lang NAME compiler_src LINK nkl_lang TARGET SRC_TEST)

target_link_options(${AST_TEST} PRIVATE -export-dynamic)
target_link_options(${SRC_TEST} PRIVATE -export-dynamic)

target_compile_definitions(${AST_TEST} PRIVATE CONFIG_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
target_compile_definitions(${SRC_TEST} PRIVATE CONFIG_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
