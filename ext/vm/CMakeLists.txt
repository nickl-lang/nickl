set(LIB nk_vm)

add_library(${LIB}
    src/interp.cpp
    src/ir.cpp
    src/native_adapter.cpp
    src/value.cpp
    src/vm.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nk_common
    )

if(CUSTOM_LIBFFI)
    target_link_libraries(${LIB}
        PRIVATE ${FFI_LIB}
        )
    target_include_directories(${LIB}
        PRIVATE "${FFI_INCLUDE_DIR}"
        )
else()
    find_library(FFI_LIB ffi REQUIRED)
    target_link_libraries(${LIB}
        PRIVATE ${FFI_LIB}
        )
endif()

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
