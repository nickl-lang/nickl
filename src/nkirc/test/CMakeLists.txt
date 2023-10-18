function(def_nkirc_test)
    set(options)
    set(oneValueArgs FILE ARGS PLATFORM)
    set(multiValueArgs)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_FILE)
        message(FATAL_ERROR "FILE argument is required")
    endif()

    if(ARG_PLATFORM)
        string(REGEX MATCH "${ARG_PLATFORM}" CONTINUE "${CMAKE_SYSTEM_NAME}")
        if(NOT CONTINUE)
            return()
        endif()
    endif()

    def_output_test(
        NAME nkirc.run
        FILE ${ARG_FILE}
        COMMAND "${CMAKE_CROSSCOMPILING_EMULATOR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXE}" "-krun"
        )
    def_output_test(
        NAME nkirc.compile
        FILE ${ARG_FILE}
        COMMAND
            "env"
            "EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
            "COMPILER=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${EXE}"
            "OUT_FILE=${ARG_FILE}.out"
            "${CMAKE_CURRENT_SOURCE_DIR}/compile_test.sh" "${ARG_ARGS}"
        )
endfunction()

def_nkirc_test(FILE aggregate.nkir ARGS "-lm")
def_nkirc_test(FILE basic.nkir)
def_nkirc_test(FILE casts.nkir)
# TODO def_nkirc_test(FILE extern_var.nkir)
def_nkirc_test(FILE ffi_closure.nkir)
def_nkirc_test(FILE global.nkir)
def_nkirc_test(FILE hello_world.nkir)
def_nkirc_test(FILE include.nkir)
def_nkirc_test(FILE loop.nkir)
def_nkirc_test(FILE proc.nkir)
def_nkirc_test(FILE syscall.nkir PLATFORM "Linux")
def_nkirc_test(FILE threads.nkir ARGS "-lpthread")
