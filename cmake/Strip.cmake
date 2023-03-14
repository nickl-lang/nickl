function(strip_binary)
    set(options)
    set(oneValueArgs FILE CACHE_DIR INSTALL_DIR OUT_STAMP_FILE)
    set(multiValueArgs LINK)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_FILE)
        message(FATAL_ERROR "FILE argument is required")
    endif()

    if(NOT ARG_CACHE_DIR)
        message(FATAL_ERROR "CACHE_DIR argument is required")
    endif()

    if(NOT ARG_OUT_STAMP_FILE)
        message(FATAL_ERROR "OUT_STAMP_FILE argument is required")
    endif()

    get_filename_component(DEP_FILENAME "${DEP}" NAME)
    set(OUT_FILE "${ARG_CACHE_DIR}/${DEP_FILENAME}")
    set(STAMP_FILE "${OUT_FILE}.stamp")

    if("${DEP}" IS_NEWER_THAN "${OUT_FILE}")
        file(COPY "${DEP}" DESTINATION "${ARG_CACHE_DIR}/" FOLLOW_SYMLINK_CHAIN)
    endif()

    add_custom_command(
        OUTPUT "${STAMP_FILE}"
        DEPENDS "${OUT_FILE}"
        COMMAND ${CMAKE_STRIP} "${OUT_FILE}"
        COMMAND ${CMAKE_COMMAND} -E touch "${STAMP_FILE}"
        COMMENT "Strip ${OUT_FILE}"
        VERBATIM)

    set(${OUT_STAMP_FILE} ${STAMP_FILE} PARENT_SCOPE)

    if(ARG_INSTALL_DIR)
        install(CODE
            "file(\
                INSTALL ${OUT_FILE} \
                DESTINATION ${ARG_INSTALL_DIR} FOLLOW_SYMLINK_CHAIN)"
            )
    endif()
endfunction()
