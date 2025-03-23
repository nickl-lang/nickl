set(CORELIB_DIR "../core")

configure_file(config.nkl.in ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.nkl)
install(FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.nkl DESTINATION bin)

configure_file(preload.nkl ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/preload.nkl COPYONLY)
install(FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/preload.nkl DESTINATION bin)
