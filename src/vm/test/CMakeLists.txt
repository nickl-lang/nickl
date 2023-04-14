def_test(GROUP vm NAME cc_adapter LINK ${LIB} TARGET CC_ADAPTER_TEST)
def_test(GROUP vm NAME ir LINK ${LIB} TARGET IR_TEST)
def_test(GROUP vm NAME native_fn_adapter LINK ${LIB})

target_link_options(${IR_TEST} PRIVATE -export-dynamic)

set(TEST_FILES_DIR "${CMAKE_BINARY_DIR}/test_out/")
file(MAKE_DIRECTORY "${TEST_FILES_DIR}")
target_compile_definitions(${CC_ADAPTER_TEST}
    PRIVATE TEST_CC="gcc"
    PRIVATE TEST_CC_FLAGS="-O1"
    PRIVATE TEST_FILES_DIR="${TEST_FILES_DIR}"
    PRIVATE TEST_QUIET=1
    )
