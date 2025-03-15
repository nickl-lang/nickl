set(LIB nk_vm)

add_library(${LIB}
    src/bytecode.cpp
    src/cc_adapter.c
    src/dl_adapter.cpp
    src/interp.cpp
    src/ir.cpp
    src/native_fn_adapter.cpp
    src/translate_to_c.cpp
    src/value.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC ntk
    PRIVATE libffi::libffi
    PRIVATE ${LIBDL}
    PRIVATE pthread
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()

