set(LIB nkl_core)

add_library(${LIB} SHARED
    src/compiler.cpp
    src/compiler_api.c
    src/compiler_state.cpp
    src/nickl.c
    src/search.cpp
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

install(TARGETS ${LIB}
    RUNTIME DESTINATION ${SYSTEM_INSTALL_DIR}
    )

target_compile_definitions(${LIB}
    PRIVATE LIBC_NAME="${SYSTEM_LIBC}"
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
