set(LIB nk_vmnew)

add_library(${LIB}
    src/ir.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nk_common
    PRIVATE Ffi
    PRIVATE Dl
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()

