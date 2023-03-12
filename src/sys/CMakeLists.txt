set(LIB nk_sys)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SYS_SRC
        src/linux_sys.cpp
        )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SYS_SRC
        src/windows_sys.cpp
        )
else()
    message(FATAL_ERROR "Platform is not supported")
endif()

add_library(${LIB}
    ${SYS_SRC}
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
