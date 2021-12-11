cmake_minimum_required(VERSION 3.0)
project(nickl)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

option(BUILD_TESTS "Enable testing" ON)
option(ENABLE_ASAN "Enable address sanitizer" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FLAGS "-fdiagnostics-color -Wall -Wextra -pedantic")
if(ENABLE_ASAN)
    set(FLAGS "${FLAGS} -g -fsanitize=address -fsanitize=leak")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAGS}")

if(BUILD_TESTS)
    include(Test)
    enable_testing()
    find_package(GTest REQUIRED)
endif()

add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(test)
endif()
