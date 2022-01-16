file(GLOB_RECURSE RUN_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/run/*.nkl)
foreach(FILE ${RUN_TESTS})
    def_run_test(FILE ${FILE})
endforeach()
