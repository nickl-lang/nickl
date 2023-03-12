cmake_minimum_required(VERSION 3.10)
project(nickl)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

option(BUILD_TESTS "Enable testing" OFF)
option(ENABLE_LOGGING "Enable logging" OFF)
option(ENABLE_ASAN "Enable address sanitizer" OFF)
option(ENABLE_PROFILING "Enable profiling" OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${PROJECT_SOURCE_DIR}/cmake
    ${PROJECT_SOURCE_DIR}/etc/buildenv/${PLATFORM}/cmake
    )

include(Dependencies)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

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

include(Version)
get_build_version(${PROJECT_SOURCE_DIR} BUILD_VERSION)
string(TIMESTAMP BUILD_TIME UTC)
message("Build version: ${BUILD_VERSION}")
message("Build time: ${BUILD_TIME}")

set(PACKAGE_NAME "${PROJECT_NAME}-${PLATFORM}-${BUILD_VERSION}.tar.gz")
set(PACKAGE_FILE "${PROJECT_SOURCE_DIR}/out/${PACKAGE_NAME}")

# TODO Figure out how to make it depend on installed files
add_custom_command(
    OUTPUT "${PACKAGE_FILE}"
    COMMAND find . -type f -o -type l | sed -e s/^\\.\\///g | xargs tar czf "${PACKAGE_FILE}"
    DEPENDS install
    COMMENT "Generating package '${PACKAGE_NAME}'"
    WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    VERBATIM)

add_custom_target(package DEPENDS "${PACKAGE_FILE}")

set(STDLIB_DIR ${PROJECT_SOURCE_DIR}/stdlib)
set(LIBC_NAME libc.so.6)
set(LIBM_NAME libm.so.6)
configure_file(config.nkl.in ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.nkl)

add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(test)
endif()
