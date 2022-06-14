set(TESTLIB testlib)

add_library(${TESTLIB} SHARED
    utils/testlib.cpp
    )

add_library(test_utils
    utils/test_ir.cpp
    )

target_link_libraries(test_utils
    PUBLIC ${LIB}
    )

def_test(GROUP vm NAME value LINK ${LIB})

def_test(GROUP vm NAME ir LINK ${LIB} test_utils)
def_test(GROUP vm NAME bc LINK ${LIB} test_utils)

def_test(GROUP vm NAME interp LINK ${LIB} test_utils ${TESTLIB} TARGET INTERP_TEST)

target_compile_definitions(${INTERP_TEST}
    PRIVATE TESTLIB_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/"
    PRIVATE TESTLIB_NAME="lib${TESTLIB}.so"
    )

def_test(GROUP vm NAME translate_to_c LINK ${LIB} test_utils TARGET TRANSLATE_TO_C_TEST)

find_library(LIBC libc.so.6 REQUIRED)

get_filename_component(LIBC_DIR "${LIBC}" DIRECTORY)
get_filename_component(LIBC_NAME "${LIBC}" NAME)

target_compile_definitions(${TRANSLATE_TO_C_TEST}
    PRIVATE LIBC_NAME="${LIBC_NAME}"
    PRIVATE LIBS_SEARCH_PATH="${LIBC_DIR}/"
    )

def_test(GROUP vm NAME c_compiler_adapter LINK ${LIB} test_utils TARGET C_COMPILER_ADAPTER_TEST)

set(TEST_FILES_DIR "${CMAKE_BINARY_DIR}/test_out/")
file(MAKE_DIRECTORY "${TEST_FILES_DIR}")
target_compile_definitions(${C_COMPILER_ADAPTER_TEST}
    PRIVATE LIBC_NAME="${LIBC_NAME}"
    PRIVATE LIBS_SEARCH_PATH="${LIBC_DIR}/"
    PRIVATE TEST_C_COMPILER="gcc"
    PRIVATE TEST_C_COMPILER_FLAGS=""
    PRIVATE TEST_FILES_DIR="${TEST_FILES_DIR}"
    PRIVATE TEST_QUIET=0
    )
