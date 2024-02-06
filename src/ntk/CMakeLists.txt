set(LIB ntk)

string(TOLOWER ${CMAKE_SYSTEM_NAME} PLATFORM_DIR)

add_library(${LIB}
    src/sys/${PLATFORM_DIR}/dl.c
    src/sys/${PLATFORM_DIR}/error.c
    src/sys/${PLATFORM_DIR}/file.c
    src/sys/${PLATFORM_DIR}/mem.c
    src/sys/${PLATFORM_DIR}/path.c
    src/sys/${PLATFORM_DIR}/process.c
    src/sys/${PLATFORM_DIR}/syscall.c
    src/sys/${PLATFORM_DIR}/term.c
    src/sys/${PLATFORM_DIR}/thread.c
    src/sys/${PLATFORM_DIR}/time.c
    src/allocator.c
    src/arena.c
    src/file.c
    src/id.c
    src/logger.c
    src/pipe_stream.c
    src/stream.c
    src/string.c
    src/string_builder.c
    src/utils.c
    )

if(ENABLE_LOGGING)
    target_compile_definitions(${LIB}
        PUBLIC ENABLE_LOGGING
        )
endif()

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PRIVATE stb
    PRIVATE Dl
    )

if(ENABLE_PROFILING)
    add_library(ntk_prof SHARED
        src/profiler.c
        )

    target_include_directories(ntk_prof
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

    target_compile_definitions(ntk_prof
        PUBLIC ENABLE_PROFILING=1
        )

    target_link_libraries(${LIB}
        PUBLIC ntk_prof
        )
endif()

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
