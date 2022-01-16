set(TEST test_${PROJECT_NAME}_ds)

add_executable(${TEST}
    array_test.cpp
    hashmap_test.cpp
    sequence_test.cpp
    )

target_link_libraries(${TEST}
    PRIVATE ${PROJECT_NAME}
    PRIVATE GTest::GTest
    PRIVATE GTest::Main
    )

gtest_discover_tests(${TEST})

set(TEST test_${PROJECT_NAME}_utils)

add_executable(${TEST}
    logger_test.cpp
    utils_test.cpp
    )

target_link_libraries(${TEST}
    PRIVATE ${PROJECT_NAME}
    PRIVATE GTest::GTest
    PRIVATE GTest::Main
    )

target_compile_definitions(${TEST}
    PRIVATE TEST_FILE_NAME="${CMAKE_CURRENT_SOURCE_DIR}/data/test_file.txt"
    PRIVATE BIG_FILE_NAME="${CMAKE_CURRENT_SOURCE_DIR}/data/big_file"
    )

gtest_discover_tests(${TEST})

set(TEST test_${PROJECT_NAME}_mem)

add_executable(${TEST}
    mem_test.cpp
    arena_test.cpp
    )

target_link_libraries(${TEST}
    PRIVATE ${PROJECT_NAME}
    PRIVATE GTest::GTest
    PRIVATE GTest::Main
    )

gtest_discover_tests(${TEST})
