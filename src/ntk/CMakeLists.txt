set(LIB ntk)

string(TOLOWER ${CMAKE_SYSTEM_NAME} SYSTEM_DIR)

set(DEPS
    ${LIBDL}
    ${LIBSHLWAPI}
    ${LIBWINPTHREAD}
    spall
    stb
    )

set(SOURCES
    src/os/${SYSTEM_DIR}/dl.c
    src/os/${SYSTEM_DIR}/error.c
    src/os/${SYSTEM_DIR}/file.c
    src/os/${SYSTEM_DIR}/mem.c
    src/os/${SYSTEM_DIR}/path.c
    src/os/${SYSTEM_DIR}/process.c
    src/os/${SYSTEM_DIR}/term.c
    src/os/${SYSTEM_DIR}/thread.c
    src/os/${SYSTEM_DIR}/time.c
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

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND SOURCES
        src/os/${SYSTEM_DIR}/syscall.c
        )
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

target_link_libraries(${LIB} ${DEPS})

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
