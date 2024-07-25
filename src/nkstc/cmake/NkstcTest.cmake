set(NKSTC_COMPILE_TEST_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/compile_test.sh")
set(NKSTC_TEST_OUT_DIR "${CMAKE_BINARY_DIR}/nkstc_test_out")

function(def_nkstc_run_test)
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
        NAME nkstc.run
        FILE ${ARG_FILE}
        WORKING_DIRECTORY "${NKSTC_TEST_OUT_DIR}"
        COMMAND
            "env"
            "LD_LIBRARY_PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
            "${CMAKE_CROSSCOMPILING_EMULATOR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXE}" "-krun"
        )
endfunction()

function(def_nkstc_compile_test)
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
        NAME nkstc.compile
        FILE ${ARG_FILE}
        WORKING_DIRECTORY "${NKSTC_TEST_OUT_DIR}"
        COMMAND
            "env"
            "LD_LIBRARY_PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
            "EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
            "COMPILER=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXE}"
            "OUT_FILE=${BASE_NAME}.out"
            "${NKSTC_COMPILE_TEST_SCRIPT}" "${ARG_ARGS}"
        )
endfunction()
