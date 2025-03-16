function(get_build_version REPO_DIR OUT_VERSION)
    find_program(GIT git)
    if(GIT)
        execute_process(
            COMMAND ${GIT} describe --tags --dirty
            WORKING_DIRECTORY ${REPO_DIR}
            RESULT_VARIABLE RESULT
            OUTPUT_VARIABLE VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT ${RESULT} EQUAL 0)
            message(WARNING "Repo tag not found")
            set(VERSION "unknown")
        endif()
    else()
        message(WARNING "Git not found")
        set(VERSION "unknown")
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        string(APPEND VERSION "-debug")
    endif()

    set(${OUT_VERSION} ${VERSION} PARENT_SCOPE)
endfunction()
