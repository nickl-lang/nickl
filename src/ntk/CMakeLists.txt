set(LIB ntk)

set(SOURCES
    src/allocator.c
    src/arena.c
    src/atom.c
    src/error.c
    src/file.c
    src/log.c
    src/path.c
    src/pipe_stream.c
    src/profiler.c
    src/stream.c
    src/string.c
    src/string_builder.c
    src/time.c
    src/utils.c
    )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND SOURCES
        src/os/posix/dl.c
        src/os/posix/error.c
        src/os/posix/file.c
        src/os/posix/mem.c
        src/os/posix/path.c
        src/os/posix/process.c
        src/os/posix/term.c
        src/os/posix/thread.c
        src/os/posix/time.c
        )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND SOURCES
        src/os/linux/dl.c
        src/os/linux/path.c
        src/os/linux/syscall.c
        src/os/linux/time.c
        )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND SOURCES
        src/os/darwin/dl.c
        src/os/darwin/path.c
        src/os/darwin/time.c
        )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND SOURCES
        src/os/windows/dl.c
        src/os/windows/error.c
        src/os/windows/file.c
        src/os/windows/mem.c
        src/os/windows/path.c
        src/os/windows/process.c
        src/os/windows/term.c
        src/os/windows/thread.c
        src/os/windows/time.c
        )
else()
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} system is not supported")
endif()

add_library(${LIB} SHARED
    ${SOURCES}
    )

if(ENABLE_LOGGING)
    target_compile_definitions(${LIB}
        PUBLIC ENABLE_LOGGING=1
        )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(${LIB}
        PRIVATE _GNU_SOURCE
        )
endif()

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PRIVATE ${LIBDL}
    PRIVATE ${LIBSHLWAPI}
    PRIVATE ${LIBWINPTHREAD}
    PRIVATE spall
    PRIVATE stb
    )

if(ENABLE_PROFILING)
    target_compile_definitions(${LIB}
        PUBLIC ENABLE_PROFILING=1
        )
endif()

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()

install(TARGETS ${LIB}
    RUNTIME DESTINATION ${SYSTEM_INSTALL_DIR}
    )
