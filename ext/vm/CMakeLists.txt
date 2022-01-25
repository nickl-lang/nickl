set(LIB nk_vm)

add_library(${LIB}
    src/ir.cpp
    src/value.cpp
    src/vm.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nk_common
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
