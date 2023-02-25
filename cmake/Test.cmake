function(def_test)
    set(options)
    set(oneValueArgs GROUP NAME TARGET)
    set(multiValueArgs LINK)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(TEST_NAME "")

    if(ARG_GROUP)
        set(TEST_NAME ${ARG_GROUP}_)
    endif()

    if(NOT ARG_NAME)
        message(FATAL_ERROR "NAME argument is required")
    endif()

    set(TEST_NAME ${TEST_NAME}${ARG_NAME})
    set(TARGET_NAME test_${TEST_NAME})

    if(ARG_TARGET)
        set(${ARG_TARGET} ${TARGET_NAME} PARENT_SCOPE)
    endif()

    add_executable(${TARGET_NAME} ${ARG_NAME}_test.cpp)

    target_include_directories(${TARGET_NAME}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src"
        )

    target_link_libraries(${TARGET_NAME}
        GTest::GTest
        GTest::Main
        ${ARG_LINK}
        )

    gtest_discover_tests(${TARGET_NAME})
endfunction()

function(def_run_test)
    set(options)
    set(oneValueArgs NAME OUTPUT_REGEX)
    set(multiValueArgs)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "NAME argument is required")
    endif()

    get_filename_component(ABS_FILE ${ARG_NAME}.nkl ABSOLUTE)

    add_test(
        NAME run.${ARG_NAME}
        COMMAND ${PROJECT_NAME} ${ABS_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

    if(ARG_OUTPUT_REGEX)
        set_tests_properties(run.${ARG_NAME} PROPERTIES PASS_REGULAR_EXPRESSION ${ARG_OUTPUT_REGEX})
    endif()
endfunction()

function(def_compile_test)
    set(options)
    set(oneValueArgs NAME OUTPUT_REGEX)
    set(multiValueArgs)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "NAME argument is required")
    endif()

    get_filename_component(ABS_FILE ${ARG_NAME}.nkl ABSOLUTE)

    set(TEST_OUT_DIR "${CMAKE_BINARY_DIR}/compile_test_out")
    make_directory("${TEST_OUT_DIR}")
    add_test(
        NAME compile.${ARG_NAME}
        COMMAND sh -c "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME} ${ABS_FILE} && ./${ARG_NAME}"
        WORKING_DIRECTORY "${TEST_OUT_DIR}"
        )

    if(ARG_OUTPUT_REGEX)
        set_tests_properties(compile.${ARG_NAME} PROPERTIES PASS_REGULAR_EXPRESSION ${ARG_OUTPUT_REGEX})
    endif()
endfunction()
