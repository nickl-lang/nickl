set(LIB nk_common)

add_library(${LIB}
    src/arena.cpp
    src/file_utils.cpp
    src/logger.cpp
    src/mem.cpp
    src/utils.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

if(ENABLE_LOGGING)
    target_compile_definitions(${LIB}
        PUBLIC ENABLE_LOGGING
        )
endif()

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
