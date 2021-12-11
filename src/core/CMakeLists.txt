set(LIB nkl_core)

add_library(${LIB}
    src/core.cpp
    src/id.cpp
    src/value.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nk_common
    )

if(BUILD_TESTS)
    add_subdirectory(test)
endif()
