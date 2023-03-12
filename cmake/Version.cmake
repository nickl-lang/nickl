function(get_build_version REPO_DIR OUT_VERSION)
    set(REPO_TAG "UNDEFINED")

    find_program(GIT git)
    if(GIT)
        execute_process(
            COMMAND ${GIT} describe --tags --dirty
            WORKING_DIRECTORY ${REPO_DIR}
            OUTPUT_VARIABLE REPO_TAG
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    if(NOT REPO_TAG)
            message(WARNING "Repo tag is not found")
        endif()
    else()
        message(WARNING "Git is not found")
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        string(APPEND REPO_TAG "-debug")
    endif()

    if(DEV_BUILD)
        string(APPEND REPO_TAG "-dev")
    endif()

    set(${OUT_VERSION} ${REPO_TAG} PARENT_SCOPE)
endfunction()
