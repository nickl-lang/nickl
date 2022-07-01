set(TESTLIB testlib)

add_library(${TESTLIB} SHARED
    utils/testlib.c
    )

def_test(GROUP vm NAME c_compiler_adapter LINK ${LIB} TARGET C_COMPILER_ADAPTER_TEST)
def_test(GROUP vm NAME so_adapter LINK ${LIB} ${TESTLIB} TARGET SO_ADAPTER_TEST)
def_test(GROUP vm NAME value LINK ${LIB})

set(TEST_FILES_DIR "${CMAKE_BINARY_DIR}/test_out/")
file(MAKE_DIRECTORY "${TEST_FILES_DIR}")
target_compile_definitions(${C_COMPILER_ADAPTER_TEST}
    PRIVATE TEST_C_COMPILER="gcc"
    PRIVATE TEST_C_COMPILER_FLAGS=""
    PRIVATE TEST_FILES_DIR="${TEST_FILES_DIR}"
    PRIVATE TEST_QUIET=1
    )

target_compile_definitions(${SO_ADAPTER_TEST}
    PRIVATE TESTLIB="$<TARGET_FILE:${TESTLIB}>"
    )
