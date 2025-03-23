if(DEFINED ENV{CMAKE_TOOLCHAIN_FILE})
    include($ENV{CMAKE_TOOLCHAIN_FILE})
endif()

if(DEFINED CMAKE_SYSTEM_NAME)
    execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${CMAKE_SYSTEM_NAME}")
else()
    execute_process(COMMAND uname -s)
endif()
