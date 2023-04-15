cmake_minimum_required(VERSION 3.15)
project(nickl)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

option(DEV_BUILD "Development build" OFF)
option(BUILD_TESTS "Enable testing" OFF)
option(ENABLE_LOGGING "Enable logging" OFF)
option(ENABLE_ASAN "Enable address sanitizer" OFF)
option(ENABLE_PROFILING "Enable profiling" OFF)

if(CMAKE_CROSSCOMPILING AND BUILD_TESTS)
    message("Cross-compiling. Tests are disabled")
    set(BUILD_TESTS OFF)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${PROJECT_SOURCE_DIR}/cmake
    ${PROJECT_SOURCE_DIR}/etc/buildenv/${PLATFORM}/cmake
    )

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(Dependencies)

set(FLAGS "-fdiagnostics-color -Wall -Wextra -pedantic -Werror=unused-result -fvisibility=hidden")
if(ENABLE_ASAN)
    set(FLAGS "${FLAGS} -g -fsanitize=address -fsanitize=leak")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAGS} ${PLATFORM_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAGS} ${PLATFORM_CXX_FLAGS}")

if(BUILD_TESTS)
    enable_testing()
    include(Test)
    find_package(GTest REQUIRED)
endif()

if(ENABLE_PROFILING)
    find_package(easy_profiler REQUIRED)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SYSTEM_INSTALL_DIR lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SYSTEM_INSTALL_DIR bin)
else()
    message(FATAL_ERROR "Platform is not supported")
endif()

install(FILES LICENSE DESTINATION share/doc/${PROJECT_NAME}/)

include(Version)
get_build_version(${PROJECT_SOURCE_DIR} BUILD_VERSION)
string(TIMESTAMP BUILD_TIME UTC)
message("Build version: ${BUILD_VERSION}")
message("Build time: ${BUILD_TIME}")

include(Package)
add_package_target("${PROJECT_NAME}-${PLATFORM}-${BUILD_VERSION}" "${DEPLOY_PREFIX}")

add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(test)
endif()
