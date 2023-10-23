set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

add_subdirectory(config)

set(EXE nkirc)

add_executable(${EXE}
    src/irc.cpp
    src/lexer.cpp
    src/main.cpp
    src/parser.cpp
    src/types.cpp
    )

target_link_libraries(${EXE}
    PRIVATE nkb
    PRIVATE nkl_common
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
    include(NkircTest)
    add_subdirectory(test)
endif()
