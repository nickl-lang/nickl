set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(EXE nkstc)

add_executable(${EXE}
    src/lexer.cpp
    src/main.cpp
    src/parser.cpp
    src/stc.cpp
    )

target_link_libraries(${EXE}
    PRIVATE nkb
    PRIVATE nkl_common
    PRIVATE nkl_core
    )

install(TARGETS ${EXE})

target_compile_definitions(${EXE}
    PUBLIC NK_BINARY_NAME="${EXE}"
    PUBLIC NK_BUILD_VERSION="${BUILD_VERSION}"
    PUBLIC NK_BUILD_TIME="${BUILD_TIME}"
    )

if(CMAKE_TESTING_ENABLED)
    include(NkstcTest)
    add_subdirectory(test)
endif()
