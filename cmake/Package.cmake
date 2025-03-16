function(add_package_target PKG_NAME OUT_DIR)
    make_directory("${CMAKE_INSTALL_PREFIX}")
    make_directory("${OUT_DIR}")

    set(PKG_FILENAME "${PKG_NAME}.tar.gz")
    set(PKG_FILE "${OUT_DIR}/${PKG_FILENAME}")

    add_custom_target(package
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --target install/strip
        COMMAND find . -type f -o -type l | sed -e s/^\\.\\///g | xargs tar czf "${PKG_FILE}" --owner=0 --group=0
        COMMENT "Generating package '${PKG_FILENAME}'"
        WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
        VERBATIM)
endfunction()
