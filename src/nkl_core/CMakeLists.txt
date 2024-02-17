set(LIB nkl_core)

add_library(${LIB}
    src/compiler.cpp
    src/types.c
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nkb
    PUBLIC nkl_common
    PUBLIC ntk
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
