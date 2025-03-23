set(CMAKE_CXX_FLAGS "")
set(CMAKE_C_FLAGS "-g -O0")
if(ENABLE_ASAN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fsanitize=address")
endif()

set(LIB test_import_lib)

add_library(${LIB} SHARED lib.c)
target_link_libraries(${LIB} PRIVATE ntk)

set_target_properties(${LIB} PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${NKIRC_TEST_OUT_DIR}"
   LIBRARY_OUTPUT_DIRECTORY "${NKIRC_TEST_OUT_DIR}"
   )

def_nkirc_run_test(FILE import.nkir)

set(NKIRC_COMPILE_ARGS "-L${NKIRC_TEST_OUT_DIR} -l${LIB}")
if(ENABLE_ASAN)
    set(NKIRC_COMPILE_ARGS "${NKIRC_COMPILE_ARGS} -g --asan")
endif()
def_nkirc_compile_test(FILE import.nkir ARGS "${NKIRC_COMPILE_ARGS}")
