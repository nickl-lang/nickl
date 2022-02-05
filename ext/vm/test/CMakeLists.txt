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

def_test(GROUP vm NAME c_compiler LINK ${LIB} test_utils)
