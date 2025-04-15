set(CMAKE_SYSTEM_NAME Darwin)

set(PREFIX "/opt/toolchain")
set(TARGET "$ENV{TARGET}")

if("${TARGET}" STREQUAL "")
    message(FATAL_ERROR "Specify TARGET")
endif()

string(REGEX MATCH "^[a-zA-Z0-9_]+" CMAKE_SYSTEM_PROCESSOR "${TARGET}")

set(CMAKE_C_COMPILER "${PREFIX}/bin/${TARGET}-cc")
set(CMAKE_CXX_COMPILER "${PREFIX}/bin/${TARGET}-c++")

set(CMAKE_LINKER "${PREFIX}/bin/${TARGET}-ld" CACHE FILEPATH "ld")
set(CMAKE_AR "${PREFIX}/bin/${TARGET}-ar" CACHE FILEPATH "ar")
set(CMAKE_NM "${PREFIX}/bin/${TARGET}-nm" CACHE FILEPATH "nm")
set(CMAKE_RANLIB "${PREFIX}/bin/${TARGET}-ranlib" CACHE FILEPATH "ranlib")
set(CMAKE_STRIP "${PREFIX}/bin/${TARGET}-strip" CACHE FILEPATH "strip")
set(CMAKE_INSTALL_NAME_TOOL "${PREFIX}/bin/${TARGET}-install_name_tool" CACHE FILEPATH "install_name_tool")

set(CMAKE_FIND_ROOT_PATH
    "${PREFIX}"
    "${PREFIX}/SDK"
    )

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS "-isystem ${PREFIX}/include ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "-isystem ${PREFIX}/include ${CMAKE_CXX_FLAGS}")
