set(LIB spall)

add_library(${LIB} INTERFACE)

target_include_directories(${LIB} SYSTEM
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
