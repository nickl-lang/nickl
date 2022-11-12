include(GoogleTest)

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
        Gtest
        Gtest_Main
        ${Threads_LIBRARY}
        ${ARG_LINK}
        )

    gtest_add_tests(${TARGET_NAME} "" AUTO)
endfunction()

function(def_run_test)
    set(options)
    set(oneValueArgs FILE)
    set(multiValueArgs)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_FILE)
        message(FATAL_ERROR "FILE argument is required")
    endif()

    get_filename_component(TEST_NAME ${ARG_FILE} NAME)
    get_filename_component(ABS_FILE ${ARG_FILE} ABSOLUTE)
    add_test(NAME run.${TEST_NAME} COMMAND ${PROJECT_NAME} ${ABS_FILE})
endfunction()
