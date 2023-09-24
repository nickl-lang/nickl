set(LIB nk_sys)

string(TOLOWER ${CMAKE_SYSTEM_NAME} PLATFORM_DIR)

add_library(${LIB}
    src/${PLATFORM_DIR}/app.cpp
    src/${PLATFORM_DIR}/error.c
    src/${PLATFORM_DIR}/file.c
    src/${PLATFORM_DIR}/mem.c
    src/${PLATFORM_DIR}/process.c
    src/${PLATFORM_DIR}/term.c
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
