def_test(GROUP vm NAME cc_adapter LINK ${LIB} TARGET CC_ADAPTER_TEST)
def_test(GROUP vm NAME ir LINK ${LIB} TARGET IR_TEST)
def_test(GROUP vm NAME native_fn_adapter LINK ${LIB})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(${IR_TEST} PRIVATE -export-dynamic)
    set(TEST_EXECUTABLE_EXT "")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_options(${IR_TEST} PRIVATE -export-all-symbols)
    set(TEST_EXECUTABLE_EXT ".exe")
else()
    message(FATAL_ERROR "Platform is not supported")
endif()

set(TEST_FILES_DIR "${CMAKE_BINARY_DIR}/test_out/")
file(MAKE_DIRECTORY "${TEST_FILES_DIR}")
target_compile_definitions(${CC_ADAPTER_TEST}
    PRIVATE TEST_CC="gcc"
    PRIVATE TEST_CC_FLAGS="-O1"
    PRIVATE TEST_FILES_DIR="${TEST_FILES_DIR}"
    PRIVATE TEST_QUIET=0
    PRIVATE TEST_EXECUTABLE_EXT="${TEST_EXECUTABLE_EXT}"
    )
