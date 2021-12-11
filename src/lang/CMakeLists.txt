set(LIB nkl_lang)

add_library(${LIB}
    src/ast.cpp
    src/ir.cpp
    src/lexer.cpp
    src/parser.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nkl_core
    )

if(BUILD_TESTS)
    add_subdirectory(test)
endif()
