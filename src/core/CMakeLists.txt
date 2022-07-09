set(LIB nkl_core)

add_library(${LIB}
    src/ast.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nk_common
    PUBLIC nk_vm
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
