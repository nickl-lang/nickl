set(CMAKE_CXX_FLAGS "")
set(CMAKE_C_FLAGS "-g -O0")

set(LIB test_import_lib)

add_library(${LIB} SHARED lib.c)
target_link_libraries(${LIB} PRIVATE ntk)

def_nkirc_run_test(FILE import.nkir)
def_nkirc_compile_test(FILE import.nkir ARGS "-L. -l${LIB}")
