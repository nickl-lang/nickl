set(EXE nickl)

add_library(nkl_lang
    src/ast.cpp
    src/compiler.cpp
    src/lexer.cpp
    src/parser.cpp
    src/value.cpp
    )

target_include_directories(nkl_lang
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(nkl_lang
    PUBLIC ntk
    PUBLIC nk_vm
    )

add_library(nkl SHARED
    src/main.cpp
    )

target_compile_definitions(nkl
    PUBLIC NKL_BINARY_NAME="${EXE}"
    PUBLIC NKL_BUILD_VERSION="${BUILD_VERSION}"
    PUBLIC NKL_BUILD_TIME="${BUILD_TIME}"
    )

target_link_libraries(nkl
    PUBLIC nkl_lang
    )

install(TARGETS nkl
    RUNTIME DESTINATION ${SYSTEM_INSTALL_DIR}
    )

add_executable(${EXE}
    src/entry.cpp
    )

target_link_libraries(${EXE}
    PRIVATE nkl
    )

install(TARGETS ${EXE})

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
