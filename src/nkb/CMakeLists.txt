set(LIB nkb)

add_library(${LIB}
    src/bytecode.cpp
    src/cc_adapter.c
    src/common.cpp
    src/ffi_adapter.cpp
    src/interp.cpp
    src/ir.cpp
    src/translate2c.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC ntk
    PRIVATE Ffi
    PRIVATE Dl
    PRIVATE pthread
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
