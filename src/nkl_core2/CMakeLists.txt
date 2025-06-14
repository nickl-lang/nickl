set(LIB nkl_core2)

add_library(${LIB} SHARED
    src/ast_parser.c
    src/ir_parser.c
    src/lexer.c
    src/nickl.c
    src/nickl_impl.c
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nkl_common
    PUBLIC ntk
    PUBLIC nkb2
    )

install(TARGETS ${LIB}
    RUNTIME DESTINATION ${SYSTEM_INSTALL_DIR}
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
