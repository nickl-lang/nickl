set(EXE nkstc)

add_executable(${EXE}
    src/main.cpp
    src/stc.cpp
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
