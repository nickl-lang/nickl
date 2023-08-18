cmake_minimum_required(VERSION 3.15)
project(vmnew)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)
include(NewTest)

set(LIB nk_vmnew)

add_library(${LIB}
    src/ir.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nk_common
    PRIVATE Ffi
    PRIVATE Dl
    )

set(EXE nickl_irc)

add_executable(${EXE}
    src/main.cpp
    )

target_link_libraries(${EXE}
    PRIVATE nk_vmnew
    )

target_compile_definitions(${EXE}
    PUBLIC NKL_BINARY_NAME="${EXE}"
    PUBLIC NKL_BUILD_VERSION="${BUILD_VERSION}"
    PUBLIC NKL_BUILD_TIME="${BUILD_TIME}"
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
