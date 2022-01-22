set(LIB nk_vm)

add_library(${LIB}
    src/vm.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
