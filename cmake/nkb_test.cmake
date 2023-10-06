set(NKB_TEST_OUT_DIR "${CMAKE_BINARY_DIR}/nkb_test_out")
set(NKB_TEST_SCRIPT "${PROJECT_SOURCE_DIR}/cmake/nkb_test.sh")

function(def_nkb_test)
    set(options)
    set(oneValueArgs FILE)
    set(multiValueArgs)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_FILE)
        message(FATAL_ERROR "FILE argument is required")
    endif()

    get_filename_component(BASE_NAME "${ARG_FILE}" NAME_WE)
    get_filename_component(TEST_FILE "${ARG_FILE}" ABSOLUTE)

    make_directory("${NKB_TEST_OUT_DIR}")
    add_test(
        NAME nkb.run.${BASE_NAME}
        COMMAND "${NKB_TEST_SCRIPT}"
            "--file=${TEST_FILE}"
            "--exe=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXE}"
            "--emulator=${CMAKE_CROSSCOMPILING_EMULATOR}"
            "--mode=run"
            WORKING_DIRECTORY "${NKB_TEST_OUT_DIR}"
        )
    add_test(
        NAME nkb.compile.${BASE_NAME}
        COMMAND "${NKB_TEST_SCRIPT}"
            "--file=${TEST_FILE}"
            "--exe=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXE}"
            "--emulator=${CMAKE_CROSSCOMPILING_EMULATOR}"
            "--mode=compile"
            WORKING_DIRECTORY "${NKB_TEST_OUT_DIR}"
        )
endfunction()
