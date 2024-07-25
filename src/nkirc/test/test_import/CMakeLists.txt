set(CMAKE_CXX_FLAGS "")
set(CMAKE_C_FLAGS "-g -O0")

set(LIB test_import_lib)

add_library(${LIB} SHARED lib.c)
target_link_libraries(${LIB} PRIVATE ntk)

set_target_properties(${LIB} PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${NKIRC_TEST_OUT_DIR}"
   LIBRARY_OUTPUT_DIRECTORY "${NKIRC_TEST_OUT_DIR}"
   )

def_nkirc_run_test(FILE import.nkir)
def_nkirc_compile_test(FILE import.nkir ARGS "-L${NKIRC_TEST_OUT_DIR} -l${LIB}")
