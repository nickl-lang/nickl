cmake_minimum_required(VERSION 3.15)
project(nickl)

##### Common #####

option(BUILD_TESTS "Enable testing" OFF)
option(ENABLE_LOGGING "Enable logging" OFF)
option(ENABLE_ASAN "Enable address sanitizer" OFF)
option(ENABLE_PROFILING "Enable profiling" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ccache
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using Ccache")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
else()
    message(STATUS "Ccache not found")
endif()

##### Dependencies #####

list(APPEND CMAKE_PREFIX_PATH /opt/toolchain)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBFFI REQUIRED IMPORTED_TARGET libffi)
add_library(libffi::libffi ALIAS PkgConfig::LIBFFI)

find_library(LIBDL NAMES dl REQUIRED)

if(BUILD_TESTS)
    enable_testing()

    include(Test)
    include(OutputTest)

    find_package(GTest REQUIRED)
endif()

##### Flags #####

set(FLAGS)

set(FLAGS "${FLAGS} -Wall")
set(FLAGS "${FLAGS} -Wextra")
set(FLAGS "${FLAGS} -pedantic")
set(FLAGS "${FLAGS} -Werror=unused-result")
set(FLAGS "${FLAGS} -Werror=format")
set(FLAGS "${FLAGS} -Werror=switch")

set(FLAGS "${FLAGS} -fvisibility=hidden")

set(FLAGS "${FLAGS} -fdiagnostics-color")

set(FLAGS "${FLAGS} -static-libstdc++")
set(FLAGS "${FLAGS} -static-libgcc")

if(ENABLE_ASAN)
    set(FLAGS "${FLAGS} -g")
    set(FLAGS "${FLAGS} -fsanitize=address")
    set(FLAGS "${FLAGS} -fsanitize=leak")
endif()

set(CXX_FLAGS "${FLAGS}")

set(C_FLAGS "${FLAGS}")
set(C_FLAGS "${C_FLAGS} -Werror=implicit-function-declaration")
set(C_FLAGS "${C_FLAGS} -Werror=implicit-int")
set(C_FLAGS "${C_FLAGS} -Werror=int-conversion")
set(C_FLAGS "${C_FLAGS} -Werror=int-to-pointer-cast")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS}")

##### Targets #####

include(Version)
get_build_version(${PROJECT_SOURCE_DIR} BUILD_VERSION)
string(TIMESTAMP BUILD_TIME UTC)
message(STATUS "Build version: ${BUILD_VERSION}")
message(STATUS "Build time: ${BUILD_TIME}")

include(Package)
add_package_target(
    "${PROJECT_NAME}-${PLATFORM}-${BUILD_VERSION}"
    "${DEPLOY_PREFIX}"
    )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SYSTEM_INSTALL_DIR lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SYSTEM_INSTALL_DIR bin)
    add_compile_definitions(__USE_MINGW_ANSI_STDIO=1)
else()
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} system is not supported")
endif()

add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(test)
endif()

install(FILES LICENSE DESTINATION share/doc/${PROJECT_NAME}/)
