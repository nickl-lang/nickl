cmake_minimum_required(VERSION 3.15)
project(nickl)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

option(DEV_BUILD "Development build" OFF)
option(BUILD_TESTS "Enable testing" OFF)
option(ENABLE_LOGGING "Enable logging" OFF)
option(ENABLE_ASAN "Enable address sanitizer" OFF)
option(ENABLE_PROFILING "Enable profiling" OFF)

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
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(Dependencies)

# TODO Organize flags better
set(FLAGS "-fdiagnostics-color -Wall -Wextra -pedantic -Werror=unused-result -Werror=format -fvisibility=hidden")
if(ENABLE_ASAN)
    set(FLAGS "${FLAGS} -g -fsanitize=address -fsanitize=leak")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAGS} ${PLATFORM_CXX_FLAGS} \
    -Werror=switch"
    )
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAGS} ${PLATFORM_CXX_FLAGS} \
    -Werror=implicit-function-declaration                          \
    -Werror=implicit-int                                           \
    -Werror=int-conversion                                         \
    -Werror=int-to-pointer-cast                                    \
    -Werror=switch"
    )

if(BUILD_TESTS)
    enable_testing()
    include(Test)
    include(OutputTest)
    find_package(GTest REQUIRED)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SYSTEM_INSTALL_DIR lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SYSTEM_INSTALL_DIR bin)
    add_compile_definitions(__USE_MINGW_ANSI_STDIO=1)
else()
    message(FATAL_ERROR "Platform is not supported")
endif()

install(FILES LICENSE DESTINATION share/doc/${PROJECT_NAME}/)

include(Version)
get_build_version(${PROJECT_SOURCE_DIR} BUILD_VERSION)
string(TIMESTAMP BUILD_TIME UTC)
message(STATUS "Build version: ${BUILD_VERSION}")
message(STATUS "Build time: ${BUILD_TIME}")

include(Package)
add_package_target("${PROJECT_NAME}-${PLATFORM}-${BUILD_VERSION}" "${DEPLOY_PREFIX}")

# ccache
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using Ccache")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
else()
    message(STATUS "Ccache not found")
endif()

add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(test)
endif()
