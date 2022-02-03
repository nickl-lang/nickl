set(LIB nk_vm)

add_library(${LIB}
    src/interp.cpp
    src/ir.cpp
    src/native_fn_adapter.cpp
    src/value.cpp
    src/vm.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

find_library(LIBFFI ffi REQUIRED)

target_link_libraries(${LIB}
    PUBLIC nk_common
    PRIVATE ${LIBFFI}
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
