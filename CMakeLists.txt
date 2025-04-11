cmake_minimum_required(VERSION 3.15)
project(nickl)

##### Common #####

option(BUILD_TESTS "Enable testing" OFF)
option(ENABLE_LOGGING "Enable logging" OFF)
option(ENABLE_ASAN "Enable address sanitizer" OFF)
option(ENABLE_PROFILING "Enable profiling" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
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

include(FindLibraryEx)

find_package(PkgConfig REQUIRED)
pkg_check_modules_ex(LIBFFI libffi STATIC)

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
set(FLAGS "${FLAGS} -Werror=unused-result")
set(FLAGS "${FLAGS} -Werror=format")
set(FLAGS "${FLAGS} -Werror=switch")

set(FLAGS "${FLAGS} -fvisibility=hidden")

set(FLAGS "${FLAGS} -fdiagnostics-color")

if(ENABLE_ASAN)
    set(FLAGS "${FLAGS} -g")
    set(FLAGS "${FLAGS} -fsanitize=address")
endif()

if(NOT "${CMAKE_C_COMPILER_ID}" STREQUAL "${CMAKE_CXX_COMPILER_ID}")
    message(FATAL_ERROR "Don't mix compilers: CC is ${CMAKE_C_COMPILER_ID}, CXX is ${CMAKE_CXX_COMPILER_ID}")
endif()

if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
    set(FLAGS "${FLAGS} -Wno-gnu-alignof-expression")
endif()

set(CXX_FLAGS "${FLAGS}")

set(C_FLAGS "${FLAGS}")
set(C_FLAGS "${C_FLAGS} -Werror=implicit-function-declaration")
set(C_FLAGS "${C_FLAGS} -Werror=implicit-int")
set(C_FLAGS "${C_FLAGS} -Werror=int-conversion")
set(C_FLAGS "${C_FLAGS} -Werror=int-to-pointer-cast")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS}")

##### Systems #####

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SYSTEM_INSTALL_DIR lib)
    set(SYSTEM_RPATH_ORIGIN "$ORIGIN")

    set(SYSTEM_SNPRINTF_PREFIX "")

    set(SYSTEM_LIBC "libc.so.6")
    set(SYSTEM_LIBM "libm.so.6")
    set(SYSTEM_LIBPTHREAD "libpthread.so.0")
    set(SYSTEM_LIBJSONC "libjson-c.so")

    set(SYSTEM_EXPORT_DYNAMIC_FLAG "-rdynamic")

    set(SYSTEM_EXECUTABLE_EXT "")

    find_library_ex(LIBDL dl SHARED)

    set(SYSTEM_LIBRARY_PATH LD_LIBRARY_PATH)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)

    set(SYSTEM_INSTALL_DIR lib)
    set(SYSTEM_RPATH_ORIGIN "@executable_path")

    set(SYSTEM_SNPRINTF_PREFIX "")

    set(SYSTEM_LIBC "libSystem.B.dylib")
    set(SYSTEM_LIBM "libSystem.B.dylib")
    set(SYSTEM_LIBPTHREAD "libSystem.B.dylib")
    set(SYSTEM_LIBJSONC "libjson-c.dylib")

    set(SYSTEM_EXPORT_DYNAMIC_FLAG "-rdynamic")

    set(SYSTEM_EXECUTABLE_EXT "")

    set(LIBDL "")

    set(SYSTEM_LIBRARY_PATH DYLD_LIBRARY_PATH)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SYSTEM_INSTALL_DIR bin)
    set(SYSTEM_RPATH_ORIGIN "")

    set(SYSTEM_SNPRINTF_PREFIX "_")

    set(SYSTEM_LIBC "msvcrt.dll")
    set(SYSTEM_LIBM "msvcrt.dll")
    set(SYSTEM_LIBPTHREAD "libwinpthread-1.dll")
    set(SYSTEM_LIBJSONC "libjson-c.dll")

    set(SYSTEM_EXPORT_DYNAMIC_FLAG "-Wl,-export-all-symbols")

    set(SYSTEM_EXECUTABLE_EXT ".exe")

    add_compile_definitions(__USE_MINGW_ANSI_STDIO=1)

    find_library_ex(LIBDL dl SHARED)

    list(APPEND CMAKE_PREFIX_PATH ${PREFIX}/x86_64-w64-mingw32)

    find_library_ex(LIBSHLWAPI shlwapi SHARED)
    find_library_ex(LIBWINPTHREAD winpthread SHARED)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-use-cxa-atexit")
else()
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} system is not supported")
endif()

##### Targets #####

include(Version)
get_build_version(${PROJECT_SOURCE_DIR} BUILD_VERSION)
string(TIMESTAMP BUILD_TIME UTC)
message(STATUS "Build version: ${BUILD_VERSION}")
message(STATUS "Build time: ${BUILD_TIME}")

include(Package)
string(TOLOWER "${CMAKE_SYSTEM_NAME}" SYSTEM_NAME)
if(NOT DEFINED DEPLOY_PREFIX)
    set(DEPLOY_PREFIX "${CMAKE_BINARY_DIR}")
endif()
add_package_target(
    "${PROJECT_NAME}-${SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${BUILD_VERSION}"
    "${DEPLOY_PREFIX}"
    )

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_INSTALL_RPATH "${SYSTEM_RPATH_ORIGIN}/../lib")

add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(test)
endif()

install(FILES LICENSE DESTINATION share/doc/${PROJECT_NAME}/)
