set(NKIRC_COMPILE_TEST_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/compile_test.sh")
set(NKIRC_TEST_OUT_DIR "${CMAKE_BINARY_DIR}/nkirc_test_out")

function(def_nkirc_run_test)
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
        NAME nkirc.run
        FILE ${ARG_FILE}
        WORKING_DIRECTORY "${NKIRC_TEST_OUT_DIR}"
        COMMAND
            "env"
            "LD_LIBRARY_PATH=.:$ENV{LD_LIBRARY_PATH}"
            "${CMAKE_CROSSCOMPILING_EMULATOR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXE}" "-krun"
        )
endfunction()

function(def_nkirc_compile_test)
    set(options)
    set(oneValueArgs FILE ARGS PLATFORM)
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

    get_filename_component(BASE_NAME "${ARG_FILE}" NAME_WE)

    def_output_test(
        NAME nkirc.compile
        FILE ${ARG_FILE}
        WORKING_DIRECTORY "${NKIRC_TEST_OUT_DIR}"
        COMMAND
            "env"
            "LD_LIBRARY_PATH=.:$ENV{LD_LIBRARY_PATH}"
            "EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
            "COMPILER=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXE}"
            "OUT_FILE=${BASE_NAME}.out"
            "${NKIRC_COMPILE_TEST_SCRIPT}" "${ARG_ARGS}"
        )
endfunction()
