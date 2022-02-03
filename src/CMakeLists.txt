set(LIB nkl_core)

add_library(${LIB}
    src/ast.cpp
    src/core.cpp
    src/id.cpp
    src/ir.cpp
    src/lexer.cpp
    src/parser.cpp
    src/value.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nk_common
    )

set(EXE nickl)

add_executable(${EXE}
    src/main.cpp
    )

install(TARGETS ${EXE})

target_compile_definitions(${EXE}
    PUBLIC NKL_BINARY_NAME="${EXE}"
    PUBLIC NKL_VERSION_MAJOR=0
    PUBLIC NKL_VERSION_MINOR=0
    PUBLIC NKL_VERSION_PATCH=0
    )

target_link_libraries(${EXE}
    PRIVATE nkl_core
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
