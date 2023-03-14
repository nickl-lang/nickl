if(ENABLE_LOGGING)
    add_library(nk_logger SHARED
        src/logger.cpp
        )

    target_compile_definitions(nk_logger
        PUBLIC ENABLE_LOGGING
        )

    target_link_libraries(nk_logger
        PUBLIC pthread # TODO Check if we need pthread here
        PRIVATE nk_sys
        )

    target_include_directories(nk_logger
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
else()
    add_library(nk_logger INTERFACE)

    target_include_directories(nk_logger
        INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
endif()

install(TARGETS nk_logger)

add_library(nk_common
    src/allocator.cpp
    src/id.cpp
    src/string.cpp
    src/string_builder.cpp
    src/utils.cpp
    )

target_include_directories(nk_common
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(nk_common
    PUBLIC nk_logger
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
