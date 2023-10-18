set(LIB test_extern_lib)

add_library(${LIB} SHARED
    lib.c
    )

target_link_libraries(${LIB}
    PRIVATE ntk
    )
