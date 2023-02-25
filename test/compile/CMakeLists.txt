def_compile_test(NAME simple)
def_compile_test(NAME printf OUTPUT_REGEX "^hello world\n$")

configure_file(config.txt ${COMPILE_TEST_OUT_DIR}/config.txt COPYONLY)
def_compile_test(NAME configured OUTPUT_REGEX "^main\ni=0\ni=1\ni=2\n$")
