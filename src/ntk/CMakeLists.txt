set(LIB ntk)

string(TOLOWER ${CMAKE_SYSTEM_NAME} PLATFORM_DIR)

set(SOURCES
    src/os/${PLATFORM_DIR}/dl.c
    src/os/${PLATFORM_DIR}/error.c
    src/os/${PLATFORM_DIR}/file.c
    src/os/${PLATFORM_DIR}/mem.c
    src/os/${PLATFORM_DIR}/path.c
    src/os/${PLATFORM_DIR}/process.c
    src/os/${PLATFORM_DIR}/term.c
    src/os/${PLATFORM_DIR}/thread.c
    src/os/${PLATFORM_DIR}/time.c
    src/allocator.c
    src/arena.c
    src/atom.c
    src/error.c
    src/file.c
    src/log.c
    src/pipe_stream.c
    src/profiler.c
    src/stream.c
    src/string.c
    src/string_builder.c
    src/time.c
    src/utils.c
    )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND SOURCES
        src/os/${PLATFORM_DIR}/syscall.c
    )
endif()

add_library(${LIB} ${SOURCES})

if(ENABLE_LOGGING)
    target_compile_definitions(${LIB}
        PUBLIC ENABLE_LOGGING=1
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
    target_compile_definitions(${LIB}
        PUBLIC ENABLE_PROFILING=1
        )
endif()

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
