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

include(Strip)
set(STRIP_STAMPS)
foreach(DEP ${SYSTEM_DEPS})
    strip_binary(
        FILE "${DEP}"
        CACHE_DIR "${CMAKE_BINARY_DIR}/system_deps"
        INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${SYSTEM_INSTALL_DIR}"
        OUT_STAMP_FILE STAMP_FILE
        )
    list(APPEND STRIP_STAMPS "${STAMP_FILE}")
endforeach()
add_custom_target(strip_system_deps ALL DEPENDS ${STRIP_STAMPS})

include(Version)
get_build_version(${PROJECT_SOURCE_DIR} BUILD_VERSION)
string(TIMESTAMP BUILD_TIME UTC)
message("Build version: ${BUILD_VERSION}")
message("Build time: ${BUILD_TIME}")

include(Package)
add_package_target("${PROJECT_NAME}-${PLATFORM}-${BUILD_VERSION}" "${DEPLOY_PREFIX}")

set(STDLIB_DIR "../stdlib")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LIBC_NAME libc.so.6)
    set(LIBM_NAME libm.so.6)

    install(
        PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/etc/scripts/${PROJECT_NAME}
        DESTINATION ${CMAKE_INSTALL_PREFIX}
        )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LIBC_NAME msvcrt.dll)
    set(LIBM_NAME msvcrt.dll)
else()
    message(FATAL_ERROR "Platform is not supported")
endif()

configure_file(config.nkl.in ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.nkl)
install(FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.nkl DESTINATION bin)

file(GLOB_RECURSE STDLIB_FILES
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stdlib/*.nkl
    )

foreach(STDLIB_FILE ${STDLIB_FILES})
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${STDLIB_FILE}
        ${CMAKE_BINARY_DIR}/${STDLIB_FILE}
        COPYONLY
        )
endforeach()

install(DIRECTORY ${CMAKE_BINARY_DIR}/stdlib DESTINATION ${CMAKE_INSTALL_PREFIX})

add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(test)
endif()
