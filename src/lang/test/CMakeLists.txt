set(TEST test_${LIB}_ast)

add_executable(${TEST}
    ast_test.cpp
    )

target_link_libraries(${TEST}
    PRIVATE ${LIB}
    PRIVATE GTest::GTest
    PRIVATE GTest::Main
    )

gtest_discover_tests(${TEST})
