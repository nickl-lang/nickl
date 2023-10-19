set(CMAKE_CXX_FLAGS "")
set(CMAKE_C_FLAGS "-g -O0")

set(LIB test_extern_lib)

add_library(${LIB} SHARED
    lib.c
    )

target_link_libraries(${LIB}
    PRIVATE ntk
    )
