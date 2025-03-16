function(add_package_target PKG_NAME OUT_DIR)
    set(PKG_FILENAME "${PKG_NAME}.tar.gz")
    set(PKG_FILE "${OUT_DIR}/${PKG_FILENAME}")
    add_custom_target(package
        COMMAND ${CMAKE_COMMAND}
            --build "${CMAKE_BINARY_DIR}"
            --target $<IF:$<CONFIG:Debug>,install,install/strip>
            --parallel
        COMMAND mkdir -p "${OUT_DIR}"
        COMMAND
            find "${CMAKE_INSTALL_PREFIX}" (-type f -o -type l)
                -exec realpath --relative-to "${CMAKE_INSTALL_PREFIX}" {} \; |
            sed -e "s/^\\.\\///g" |
            xargs tar czf "${PKG_FILE}" --owner=0 --group=0
                -C "${CMAKE_INSTALL_PREFIX}"
        COMMAND echo "Generated package '${PKG_FILE}'"
        COMMENT "Generating package '${PKG_FILENAME}'"
        VERBATIM
        )
endfunction()
