set(LIB nk_new_vm)

add_library(${LIB}
    src/ir.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

find_library(LIBFFI ffi REQUIRED)
find_library(LIBDL dl REQUIRED)

target_link_libraries(${LIB}
    PUBLIC nk_common
    PRIVATE ${LIBFFI}
    PRIVATE ${LIBDL}
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()

