set(TESTLIB testlib)

add_library(${TESTLIB} SHARED
    utils/testlib.c
    )

set(TEST_UTILS_LIB test_utils)

add_library(${TEST_UTILS_LIB}
    utils/test_ir.cpp
    )

target_link_libraries(${TEST_UTILS_LIB}
    PUBLIC ${LIB}
    )

def_test(GROUP vm NAME bc LINK ${LIB} ${TEST_UTILS_LIB})
def_test(GROUP vm NAME c_compiler_adapter LINK ${LIB} ${TEST_UTILS_LIB} TARGET C_COMPILER_ADAPTER_TEST)
def_test(GROUP vm NAME interp LINK ${LIB} ${TESTLIB} ${TEST_UTILS_LIB} TARGET INTERP_TEST)
def_test(GROUP vm NAME ir LINK ${LIB} ${TEST_UTILS_LIB})
def_test(GROUP vm NAME so_adapter LINK ${LIB} ${TESTLIB} TARGET SO_ADAPTER_TEST)
def_test(GROUP vm NAME translate_to_c LINK ${LIB} ${TEST_UTILS_LIB} TARGET TRANSLATE_TO_C_TEST)
def_test(GROUP vm NAME value LINK ${LIB})

# find_library(LIBC libc.so.6 REQUIRED)

# get_filename_component(LIBC_DIR "${LIBC}" DIRECTORY)
# get_filename_component(LIBC_NAME "${LIBC}" NAME)

set(LIBC_DIR "")
set(LIBC_NAME "")

set(TEST_FILES_DIR "${CMAKE_BINARY_DIR}/test_out/")
file(MAKE_DIRECTORY "${TEST_FILES_DIR}")
target_compile_definitions(${C_COMPILER_ADAPTER_TEST}
    PRIVATE LIBC_NAME="${LIBC_NAME}"
    PRIVATE TEST_C_COMPILER="gcc"
    PRIVATE TEST_C_COMPILER_FLAGS="-O1 -Wformat=0"
    PRIVATE TEST_FILES_DIR="${TEST_FILES_DIR}"
    PRIVATE TEST_QUIET=0
    )

target_compile_definitions(${INTERP_TEST}
    PRIVATE TESTLIB_PATH="./" #${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/"
    PRIVATE TESTLIB_NAME="lib${TESTLIB}.dll"
    )

target_compile_definitions(${SO_ADAPTER_TEST}
    PRIVATE TESTLIB="$<TARGET_FILE:${TESTLIB}>"
    )

target_compile_definitions(${TRANSLATE_TO_C_TEST}
    PRIVATE LIBC_NAME="${LIBC_NAME}"
    )
