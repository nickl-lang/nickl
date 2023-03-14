set(LIB nkl_lang)

add_library(${LIB} SHARED
    src/ast.cpp
    src/compiler.cpp
    src/lexer.cpp
    src/parser.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nk_vm
    PUBLIC nk_sys
    )

if(NOT DEV_BUILD)
    target_link_options(${LIB}
        PRIVATE -s
        )
endif()

install(TARGETS ${LIB})

set(EXE nickl)

add_executable(${EXE}
    src/main.cpp
    )

if(NOT DEV_BUILD)
    target_link_options(${EXE}
        PRIVATE -s
        )
endif()

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
