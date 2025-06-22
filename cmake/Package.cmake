function(add_package_target PKG_NAME OUT_DIR)
    if(NOT DEFINED ARTIFACTS_DIR)
        message(FATAL_ERROR "ARTIFACTS_DIR is not defined, cannot create package")
    endif()
    set(PKG_FILENAME "${PKG_NAME}.tar.gz")
    set(PKG_FILE "${OUT_DIR}/${PKG_FILENAME}")
    add_custom_target(package_install
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --target install
        )
    add_custom_target(package_install_strip
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --target install/strip
        )
    add_custom_target(package
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E tar czf "${PKG_FILE}" "${PKG_NAME}"
        COMMAND ${CMAKE_COMMAND} -E echo "Generated package '${PKG_FILE}'"
        DEPENDS $<IF:$<CONFIG:Debug>,package_install,package_install_strip>
        WORKING_DIRECTORY "${ARTIFACTS_DIR}/install"
        COMMENT "Generating package '${PKG_FILENAME}'"
        VERBATIM
        )
endfunction()
