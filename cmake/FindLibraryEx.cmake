function(find_library_ex OUTPUT_VAR LIB_NAME TYPE)
    if (TYPE STREQUAL "STATIC")
        set(CMAKE_FIND_LIBRARY_SUFFIXES .a .dll.a .so .dll .dylib)
    elseif (TYPE STREQUAL "SHARED")
        set(CMAKE_FIND_LIBRARY_SUFFIXES .so .dll .dylib .dll.a .a)
    endif()

    find_library(${OUTPUT_VAR} NAMES ${LIB_NAME} REQUIRED)

    message(STATUS "Found ${LIB_NAME}: ${${OUTPUT_VAR}}")
endfunction()

function(pkg_check_modules_ex OUTPUT_VAR LIB_NAME TYPE)
    if (TYPE STREQUAL "STATIC")
        set(CMAKE_FIND_LIBRARY_SUFFIXES .a .dll.a .so .dll .dylib)
    elseif (TYPE STREQUAL "SHARED")
        set(CMAKE_FIND_LIBRARY_SUFFIXES .so .dll .dylib .dll.a .a)
    endif()

    pkg_check_modules(${OUTPUT_VAR} REQUIRED IMPORTED_TARGET ${LIB_NAME})
    set(${OUTPUT_VAR} PkgConfig::${OUTPUT_VAR} PARENT_SCOPE)
endfunction()
