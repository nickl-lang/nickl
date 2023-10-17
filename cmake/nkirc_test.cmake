set(NKIRC_TEST_OUT_DIR "${CMAKE_BINARY_DIR}/nkirc_test_out")
set(NKIRC_TEST_SCRIPT "${PROJECT_SOURCE_DIR}/cmake/nkirc_test.sh")

function(def_nkirc_test)
    set(options)
    set(oneValueArgs FILE ARGS)
    set(multiValueArgs)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_FILE)
        message(FATAL_ERROR "FILE argument is required")
    endif()

    get_filename_component(BASE_NAME "${ARG_FILE}" NAME_WE)
    get_filename_component(TEST_FILE "${ARG_FILE}" ABSOLUTE)

    make_directory("${NKIRC_TEST_OUT_DIR}")
    add_test(
        NAME nkirc.run.${BASE_NAME}
        COMMAND "${NKIRC_TEST_SCRIPT}"
            "--file=${TEST_FILE}"
            "--exe=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXE}"
            "--emulator=${CMAKE_CROSSCOMPILING_EMULATOR}"
            "--mode=run"
            WORKING_DIRECTORY "${NKIRC_TEST_OUT_DIR}"
        )
    add_test(
        NAME nkirc.compile.${BASE_NAME}
        COMMAND "${NKIRC_TEST_SCRIPT}"
            "--file=${TEST_FILE}"
            "--exe=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXE}"
            "--emulator=${CMAKE_CROSSCOMPILING_EMULATOR}"
            "--mode=compile"
            "--args=${ARG_ARGS}"
            WORKING_DIRECTORY "${NKIRC_TEST_OUT_DIR}"
        )
endfunction()
