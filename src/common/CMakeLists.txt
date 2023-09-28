add_library(nk_common
    src/allocator.c
    src/arena.c
    src/file.cpp
    src/id.cpp
    src/logger.cpp
    src/stream.c
    src/string_builder.c
    src/utils.c
    src/utils.cpp
    )

if(ENABLE_LOGGING)
    target_compile_definitions(nk_common
        PUBLIC ENABLE_LOGGING
        )
endif()

target_include_directories(nk_common
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(nk_common
    PRIVATE stb
    PUBLIC nk_sys
    )

if(ENABLE_PROFILING)
    target_link_libraries(nk_common
        PUBLIC easy_profiler
        )

    target_compile_definitions(nk_common
        PUBLIC EASY_PROFILER_PORT=28077
        )
endif()

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
