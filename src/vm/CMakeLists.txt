set(LIB nk_vm)

add_library(${LIB}
    src/bc.cpp
    src/c_compiler_adapter.cpp
    src/find_library.cpp
    src/interp.cpp
    src/ir.cpp
    src/native_fn_adapter.cpp
    src/pipe_stream.cpp
    src/so_adapter.cpp
    src/value.cpp
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
