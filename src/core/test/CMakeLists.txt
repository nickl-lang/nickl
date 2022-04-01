def_test(GROUP core NAME ast LINK ${LIB})
def_test(GROUP core NAME compiler LINK ${LIB} TARGET COMPILER_TEST)
def_test(GROUP core NAME lexer LINK ${LIB})
def_test(GROUP core NAME parser LINK ${LIB})

find_library(LIBC c REQUIRED)

get_filename_component(LIBC_DIR "${LIBC}" DIRECTORY)
get_filename_component(LIBC_NAME "${LIBC}" NAME)

target_compile_definitions(${COMPILER_TEST}
    PRIVATE LIBC_NAME="${LIBC_NAME}.6"
    PRIVATE LIBS_SEARCH_PATH="${LIBC_DIR}/"
    )
