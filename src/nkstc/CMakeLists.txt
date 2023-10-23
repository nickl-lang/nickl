set(EXE nkstc)

add_executable(${EXE}
    src/diagnostics.cpp
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
