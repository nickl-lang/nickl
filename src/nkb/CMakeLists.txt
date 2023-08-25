set(LIB nkb)

add_library(${LIB}
    src/ir.cpp
    src/irc.cpp
    src/lexer.cpp
    src/parser.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nk_common
    PRIVATE nk_sys
    PRIVATE Ffi
    PRIVATE Dl
    )

set(EXE nkirc)

add_executable(${EXE}
    src/main.cpp
    )

target_link_libraries(${EXE}
    PRIVATE nkb
    )

target_compile_definitions(${EXE}
    PUBLIC NK_BINARY_NAME="${EXE}"
    PUBLIC NK_BUILD_VERSION="${BUILD_VERSION}"
    PUBLIC NK_BUILD_TIME="${BUILD_TIME}"
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
