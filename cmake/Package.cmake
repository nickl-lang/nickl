function(add_package_target PKG_NAME OUT_DIR)
    execute_process(
        COMMAND sh -c "tar --version"
        OUTPUT_VARIABLE TAR_VERSION
        ERROR_QUIET
        )
    if("${TAR_VERSION}" MATCHES "GNU tar")
        set(TAR_CMD tar --owner=0 --group=0 --transform=s,^.,${PKG_NAME},)
    elseif("${TAR_VERSION}" MATCHES "bsdtar")
        set(TAR_CMD tar --uid=0 --gid=0 -s /^./${PKG_NAME}/)
    else()
        message(FATAL_ERROR "Unknown tar version")
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
        COMMAND sh -c "mkdir -p '${OUT_DIR}'"
        COMMAND sh -c "${TAR_CMD} -czf '${PKG_FILE}' -C '${CMAKE_INSTALL_PREFIX}' ."
        COMMAND echo "Generated package '${PKG_FILE}'"
        DEPENDS $<IF:$<CONFIG:Debug>,package_install,package_install_strip>
        COMMENT "Generating package '${PKG_FILENAME}'"
        VERBATIM
        )
endfunction()
