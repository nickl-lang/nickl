set(EXE nklc)

add_executable(${EXE}
    src/main.c
    )

target_link_libraries(${EXE}
    PRIVATE ntk
    PRIVATE nkl_common
    # PRIVATE nkb
    # PRIVATE nkl_core
    )

install(TARGETS ${EXE})

target_compile_definitions(${EXE}
    PUBLIC NK_BINARY_NAME="${EXE}"
    PUBLIC NK_BUILD_VERSION="${BUILD_VERSION}"
    PUBLIC NK_BUILD_TIME="${BUILD_TIME}"
    )
