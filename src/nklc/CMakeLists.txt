set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(EXE nklc)

add_executable(${EXE}
    src/main.c
    )

target_link_libraries(${EXE}
    PRIVATE ntk
    PRIVATE nkl_common
    PRIVATE nkl_core2
    )

install(TARGETS ${EXE})

target_compile_definitions(${EXE}
    PUBLIC NK_BINARY_NAME="${EXE}"
    PUBLIC NK_BUILD_VERSION="${BUILD_VERSION}"
    PUBLIC NK_BUILD_TIME="${BUILD_TIME}"
    )

if(CMAKE_TESTING_ENABLED)
    include(NklcTest)
    add_subdirectory(test)
endif()
