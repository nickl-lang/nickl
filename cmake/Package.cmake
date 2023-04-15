function(add_package_target PKG_NAME OUT_DIR)
    set(PKG_FILENAME "${PKG_NAME}.tar.gz")
    set(PKG_FILE "${OUT_DIR}/${PKG_FILENAME}")
    add_custom_target(package
        COMMAND mkdir -p ${OUT_DIR}
        COMMAND find . -type f -o -type l | sed -e s/^\\.\\///g | xargs tar czf "${PKG_FILE}" --owner=0 --group=0
        DEPENDS install
        COMMENT "Generating package '${PKG_FILENAME}'"
        WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
        VERBATIM)
endfunction()
