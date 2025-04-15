def_test(GROUP lang NAME compiler_ast LINK nkl_lang TARGET AST_TEST)
def_test(GROUP lang NAME compiler_src LINK nkl_lang TARGET SRC_TEST)

target_link_options(${AST_TEST} PRIVATE ${SYSTEM_EXPORT_DYNAMIC_FLAG})
target_link_options(${SRC_TEST} PRIVATE ${SYSTEM_EXPORT_DYNAMIC_FLAG})
