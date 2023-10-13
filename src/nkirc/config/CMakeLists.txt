configure_file(nkirc.conf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nkirc.conf COPYONLY)
install(FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nkirc.conf DESTINATION bin)
