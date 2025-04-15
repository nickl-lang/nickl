configure_file(nkirc.conf.in ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nkirc.conf)
install(FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nkirc.conf DESTINATION bin)
