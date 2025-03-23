function(add_package_target PKG_NAME OUT_DIR)
    set(PKG_FILENAME "${PKG_NAME}.tar.gz")
    set(PKG_FILE "${OUT_DIR}/${PKG_FILENAME}")
    add_custom_target(package
        COMMAND mkdir -p "${OUT_DIR}"
        COMMAND bsdtar --uid=0 --gid=0 -czvf "${PKG_FILE}" -C "${CMAKE_INSTALL_PREFIX}" .
        COMMAND echo "Generated package '${PKG_FILE}'"
        DEPENDS $<IF:$<CONFIG:Debug>,install,install/strip>
        COMMENT "Generating package '${PKG_FILENAME}'"
        VERBATIM
        )
endfunction()
