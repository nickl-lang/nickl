add_subdirectory(common)
add_subdirectory(config)
add_subdirectory(lang)
add_subdirectory(nkb)
add_subdirectory(sys)
add_subdirectory(vm)

set(CORELIB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corelib)
set(CORELIB_DST_DIR ${CMAKE_BINARY_DIR}/core)

file(GLOB_RECURSE CORELIB_FILES
    RELATIVE ${CORELIB_SRC_DIR}
    ${CORELIB_SRC_DIR}/*.nkl
    )

foreach(CORELIB_FILE ${CORELIB_FILES})
    configure_file(
        ${CORELIB_SRC_DIR}/${CORELIB_FILE}
        ${CORELIB_DST_DIR}/${CORELIB_FILE}
        COPYONLY
        )
endforeach()

install(DIRECTORY ${CORELIB_DST_DIR} DESTINATION ${CMAKE_INSTALL_PREFIX})
