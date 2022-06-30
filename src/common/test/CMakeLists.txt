def_test(GROUP common NAME arena LINK ${LIB})
def_test(GROUP common NAME array LINK ${LIB})
def_test(GROUP common NAME file_utils LINK ${LIB} TARGET FILE_UTILS_TEST)
def_test(GROUP common NAME hashmap LINK ${LIB})
def_test(GROUP common NAME id LINK ${LIB})
def_test(GROUP common NAME log2arena LINK ${LIB})
def_test(GROUP common NAME mem LINK ${LIB})
def_test(GROUP common NAME slice LINK ${LIB})
def_test(GROUP common NAME stack_allocator LINK ${LIB})
def_test(GROUP common NAME static_allocator LINK ${LIB})
def_test(GROUP common NAME static_string_builder LINK ${LIB})
def_test(GROUP common NAME string_builder LINK ${LIB})
def_test(GROUP common NAME utils LINK ${LIB})

if(ENABLE_LOGGING)
    def_test(GROUP common NAME logger LINK ${LIB})
endif()

target_compile_definitions(${FILE_UTILS_TEST}
    PRIVATE TEST_FILE_NAME="${CMAKE_CURRENT_SOURCE_DIR}/data/test_file.txt"
    PRIVATE BIG_FILE_NAME="${CMAKE_CURRENT_SOURCE_DIR}/data/big_file"
    )
