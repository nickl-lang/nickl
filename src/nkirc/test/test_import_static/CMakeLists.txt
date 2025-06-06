set(CMAKE_CXX_FLAGS "")
set(CMAKE_C_FLAGS "-g -O0")

set(LIB test_import_lib_static)

add_library(${LIB} STATIC lib.c)

set_target_properties(${LIB} PROPERTIES
   ARCHIVE_OUTPUT_DIRECTORY "${NKIRC_TEST_OUT_DIR}"
   )

set(NKIRC_COMPILE_ARGS "-L${NKIRC_TEST_OUT_DIR} -l${LIB}")
def_nkirc_compile_test(FILE import_static.nkir ARGS "${NKIRC_COMPILE_ARGS}")
