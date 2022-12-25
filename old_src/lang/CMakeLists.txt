set(LIB nkl_lang)

add_library(${LIB} SHARED
    src/ast.cpp
    src/compiler.cpp
    src/lang.cpp
    src/lexer.cpp
    src/parser.cpp
    src/tokens.cpp
    src/value.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nkl_core
    )

set(EXE nickl)

add_executable(${EXE}
    src/main.cpp
    )

install(TARGETS ${EXE})

target_compile_definitions(${EXE}
    PUBLIC NKL_BINARY_NAME="${EXE}"
    PUBLIC NKL_VERSION_MAJOR=0
    PUBLIC NKL_VERSION_MINOR=1
    PUBLIC NKL_VERSION_PATCH=0
    )

target_link_libraries(${EXE}
    PRIVATE nkl_lang
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
