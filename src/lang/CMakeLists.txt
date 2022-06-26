set(LIB nkl_lang)

add_library(${LIB} INTERFACE)

target_include_directories(${LIB}
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    INTERFACE nkl_core
    )

set(EXE nickl)

add_executable(${EXE}
    src/main.cpp
    )

install(TARGETS ${EXE})

target_compile_definitions(${EXE}
    PUBLIC NKL_BINARY_NAME="${EXE}"
    PUBLIC NKL_VERSION_MAJOR=0
    PUBLIC NKL_VERSION_MINOR=1
    PUBLIC NKL_VERSION_PATCH=0
    )

target_link_libraries(${EXE}
    PRIVATE nkl_lang
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
