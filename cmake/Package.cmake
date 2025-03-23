function(add_package_target PKG_NAME OUT_DIR)
    execute_process(
        COMMAND tar --version
        OUTPUT_VARIABLE TAR_VERSION
        ERROR_QUIET
        )
    if("${TAR_VERSION}" MATCHES "GNU tar")
        set(TAR_CMD tar --owner=0 --group=0)
    elseif("${TAR_VERSION}" MATCHES "bsdtar")
        set(TAR_CMD tar --uid=0 --gid=0)
    else()
        message(FATAL_ERROR "Unknown tar version")
    endif()
    set(PKG_FILENAME "${PKG_NAME}.tar.gz")
    set(PKG_FILE "${OUT_DIR}/${PKG_FILENAME}")
    add_custom_target(package
        COMMAND mkdir -p "${OUT_DIR}"
        COMMAND ${TAR_CMD} -czf "${PKG_FILE}" -C "${CMAKE_INSTALL_PREFIX}" .
        COMMAND echo "Generated package '${PKG_FILE}'"
        DEPENDS $<IF:$<CONFIG:Debug>,install,install/strip>
        COMMENT "Generating package '${PKG_FILENAME}'"
        VERBATIM
        )
endfunction()
