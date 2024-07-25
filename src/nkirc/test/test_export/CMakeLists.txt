def_output_test(
    NAME nkirc
    FILE test_export.c
    COMMAND
        "env \
        EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR} \
        COMPILER=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXE} \
        NKIR_SRC=${CMAKE_CURRENT_SOURCE_DIR}/export.nkir \
        ${CMAKE_CURRENT_SOURCE_DIR}/test.sh"
    )
