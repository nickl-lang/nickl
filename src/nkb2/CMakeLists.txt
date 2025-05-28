set(LIB nkb2)

add_library(${LIB}
    src/ir.c
    src/llvm_stream.c
    src/types.c
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC ntk
    )
