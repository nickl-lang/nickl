set(CORELIB_DIR "../core")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LIBC_NAME libc.so.6)
    set(LIBM_NAME libm.so.6)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LIBC_NAME msvcrt.dll)
    set(LIBM_NAME msvcrt.dll)
else()
    message(FATAL_ERROR "Platform is not supported")
endif()

configure_file(config.nkl.in ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.nkl)
install(FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.nkl DESTINATION bin)

configure_file(preload.nkl ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/preload.nkl COPYONLY)
install(FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/preload.nkl DESTINATION bin)
