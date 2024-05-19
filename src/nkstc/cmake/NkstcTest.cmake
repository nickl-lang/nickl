function(def_nkstc_test)
    set(options)
    set(oneValueArgs FILE PLATFORM)
    set(multiValueArgs)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_FILE)
        message(FATAL_ERROR "FILE argument is required")
    endif()

    if(ARG_PLATFORM)
        string(REGEX MATCH "${ARG_PLATFORM}" CONTINUE "${CMAKE_SYSTEM_NAME}")
        if(NOT CONTINUE)
            return()
        endif()
    endif()

    def_output_test(
        NAME nkstc
        FILE ${ARG_FILE}
        COMMAND
            "env"
            "LD_LIBRARY_PATH=."
            "${CMAKE_CROSSCOMPILING_EMULATOR}" "./${EXE}"
        )
endfunction()
