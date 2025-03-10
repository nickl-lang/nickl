set(CORELIB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CORELIB_DST_DIR ${CMAKE_BINARY_DIR}/core)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SNPRINTF_PREFIX "")
    set(LIBPTHREAD "libpthread.so.0")
    set(LIBJSONC "libjson-c.so")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SNPRINTF_PREFIX "_")
    set(LIBPTHREAD "libwinpthread-1.dll")
    set(LIBJSONC "libjson-c.dll")
else()
    message(FATAL_ERROR "Platform is not supported")
endif()

file(GLOB_RECURSE CORELIB_FILES
    RELATIVE ${CORELIB_SRC_DIR}
    ${CORELIB_SRC_DIR}/*.nkl
    )

file(GLOB_RECURSE CORELIB_FILE_TEMPLATES
    RELATIVE ${CORELIB_SRC_DIR}
    ${CORELIB_SRC_DIR}/*.nkl.in
    )

foreach(FILE ${CORELIB_FILES})
    configure_file(
        ${CORELIB_SRC_DIR}/${FILE}
        ${CORELIB_DST_DIR}/${FILE}
        COPYONLY
        )
endforeach()

foreach(FILE ${CORELIB_FILE_TEMPLATES})
    get_filename_component(FILE_WLE ${FILE} NAME_WLE)
    configure_file(
        ${CORELIB_SRC_DIR}/${FILE}
        ${CORELIB_DST_DIR}/${FILE_WLE}
        @ONLY
        )
endforeach()

install(DIRECTORY ${CORELIB_DST_DIR} DESTINATION ${CMAKE_INSTALL_PREFIX})

