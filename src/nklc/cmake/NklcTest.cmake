set(NKLC_COMPILE_TEST_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/compile_test.sh")
set(NKLC_TEST_OUT_DIR "${CMAKE_BINARY_DIR}/nklc_test_out")

function(def_nklc_compile_test)
    set(options)
    set(oneValueArgs FILE SYSTEM)
    set(multiValueArgs ARGS)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_FILE)
        message(FATAL_ERROR "FILE argument is required")
    endif()

    if(ARG_SYSTEM)
        string(REGEX MATCH "${ARG_SYSTEM}" CONTINUE "${CMAKE_SYSTEM_NAME}")
        if(NOT CONTINUE)
            return()
        endif()
    endif()

    get_filename_component(BASE_NAME "${ARG_FILE}" NAME_WE)

    def_output_test(
        NAME nklc.compile
        FILE ${ARG_FILE}
        WORKING_DIRECTORY "${NKLC_TEST_OUT_DIR}"
        COMMAND
            env
            "${SYSTEM_LIBRARY_PATH}=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}:$ENV{${SYSTEM_LIBRARY_PATH}}"
            "EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
            "COMPILER=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXE}"
            "OUT_FILE=${BASE_NAME}.out"
            "${NKLC_COMPILE_TEST_SCRIPT}" ${ARG_ARGS}
        )
endfunction()
