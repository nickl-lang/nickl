set(LIB stb)

add_library(${LIB}
    src/sprintf.c
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
