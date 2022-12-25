set(LIB nk_common)

add_library(${LIB}
    src/allocator.cpp
    src/id.cpp
    src/logger.c
    src/string.cpp
    src/string_builder.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

if(ENABLE_LOGGING)
    target_compile_definitions(${LIB}
        PUBLIC ENABLE_LOGGING
        )

    target_link_libraries(${LIB}
        PUBLIC pthread
        )
endif()

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
