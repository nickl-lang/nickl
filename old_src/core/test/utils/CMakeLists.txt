set(LIB nkl_core_test_utils)

add_library(${LIB}
    src/ast_utils.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nkl_core
    )
