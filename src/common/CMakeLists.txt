set(LIB nk_common)

add_library(${LIB}
    src/mem/mem.cpp
    src/mem/stack_allocator.cpp
    src/mem/static_allocator.cpp
    src/str/dynamic_string_builder.cpp
    src/str/static_string_builder.cpp
    src/str/string.cpp
    src/str/string_builder.cpp
    src/utils/file_utils.cpp
    src/utils/id.cpp
    src/utils/logger.c
    src/utils/utils.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

if(ENABLE_LOGGING)
    target_compile_definitions(${LIB}
        PUBLIC ENABLE_LOGGING
        )

    target_link_libraries(${LIB}
        ${Threads_LIBRARY}
        )
endif()

if(ENABLE_PROFILING)
    target_link_libraries(${LIB}
        PUBLIC easy_profiler
        )

    target_compile_definitions(${LIB}
        PUBLIC EASY_PROFILER_PORT=28077
        )
endif()

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
