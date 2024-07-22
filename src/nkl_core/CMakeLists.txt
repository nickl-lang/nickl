set(LIB nkl_core)

add_library(${LIB}
    src/compiler.cpp
    src/compiler_state.cpp
    src/nickl.c
    src/types.c
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nkb
    PUBLIC nkl_common
    PUBLIC ntk
    )

# TODO: Hardcoded libc name
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(${LIB}
        PRIVATE LIBC_NAME=""
        )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(${LIB}
        PRIVATE LIBC_NAME="msvcrt.dll"
        )
else()
    message(FATAL_ERROR "Platform is not supported")
endif()

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
