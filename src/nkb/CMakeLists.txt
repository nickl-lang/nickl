set(LIB nkb)

add_library(${LIB}
    src/bytecode.cpp
    src/cc_adapter.c
    src/common.cpp
    src/ffi_adapter.cpp
    src/interp.cpp
    src/ir.cpp
    src/irc.cpp
    src/lexer.cpp
    src/parser.cpp
    src/types.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC ntk
    PRIVATE Ffi
    PRIVATE Dl
    PRIVATE pthread
    )

set(EXE nkirc)

add_executable(${EXE}
    src/main.cpp
    )

target_link_libraries(${EXE}
    PRIVATE nkb
    )

if(NOT DEV_BUILD)
    target_link_options(${EXE}
        PRIVATE -s
        )
endif()

install(TARGETS ${EXE})

target_compile_definitions(${EXE}
    PUBLIC NK_BINARY_NAME="${EXE}"
    PUBLIC NK_BUILD_VERSION="${BUILD_VERSION}"
    PUBLIC NK_BUILD_TIME="${BUILD_TIME}"
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test/run)
    add_subdirectory(test)
endif()
