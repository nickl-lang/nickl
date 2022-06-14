def_test(GROUP common NAME arena LINK ${LIB})
def_test(GROUP common NAME array LINK ${LIB})
def_test(GROUP common NAME hashmap LINK ${LIB})
def_test(GROUP common NAME id LINK ${LIB})
def_test(GROUP common NAME logger LINK ${LIB})
def_test(GROUP common NAME mem LINK ${LIB})
def_test(GROUP common NAME sequence LINK ${LIB})
def_test(GROUP common NAME string_builder LINK ${LIB})
def_test(GROUP common NAME utils LINK ${LIB} TARGET UTILS_TEST)

target_compile_definitions(${UTILS_TEST}
    PRIVATE TEST_FILE_NAME="${CMAKE_CURRENT_SOURCE_DIR}/data/test_file.txt"
    PRIVATE BIG_FILE_NAME="${CMAKE_CURRENT_SOURCE_DIR}/data/big_file"
    )
