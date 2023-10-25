set(LIB nkl_common)

add_library(${LIB}
    src/ast.cpp
    src/config.cpp
    src/diagnostics.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PRIVATE ntk
    )
