set(OUTPUT_TEST_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/output_test.sh")

function(def_output_test)
    set(options)
    set(oneValueArgs NAME FILE WORKING_DIRECTORY)
    set(multiValueArgs COMMAND EXTRA_ARGS)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "NAME argument is required")
    endif()

    if(NOT ARG_FILE)
        message(FATAL_ERROR "FILE argument is required")
    endif()

    if(NOT ARG_COMMAND)
        message(FATAL_ERROR "COMMAND argument is required")
    endif()

    if(NOT ARG_WORKING_DIRECTORY)
        set(ARG_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    endif()

    get_filename_component(BASE_NAME "${ARG_FILE}" NAME_WE)
    get_filename_component(TEST_FILE "${ARG_FILE}" ABSOLUTE)

    string(JOIN " " CMD ${ARG_COMMAND})
    string(JOIN " " ARGS ${ARG_EXTRA_ARGS})

    make_directory("${ARG_WORKING_DIRECTORY}")
    add_test(
        NAME ${ARG_NAME}.${BASE_NAME}
        COMMAND sh -c " \
            '${OUTPUT_TEST_SCRIPT}' \
            '--file=${TEST_FILE}' \
            '--cmd=${CMD}' \
            '--args=${ARGS}'"
        WORKING_DIRECTORY "${ARG_WORKING_DIRECTORY}"
        )
endfunction()
