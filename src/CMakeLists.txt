add_subdirectory(common)
add_subdirectory(lang)
add_subdirectory(sys)
add_subdirectory(vm)
add_subdirectory(config)

file(GLOB_RECURSE STDLIB_FILES
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/std/*.nkl
    )

foreach(STDLIB_FILE ${STDLIB_FILES})
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${STDLIB_FILE}
        ${CMAKE_BINARY_DIR}/${STDLIB_FILE}
        COPYONLY
        )
endforeach()

install(DIRECTORY ${CMAKE_BINARY_DIR}/std DESTINATION ${CMAKE_INSTALL_PREFIX})
