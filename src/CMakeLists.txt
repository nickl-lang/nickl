add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(vm)

add_executable(${PROJECT_NAME}
    main.cpp
    )

target_compile_definitions(${PROJECT_NAME}
    PUBLIC NKL_BINARY_NAME="${PROJECT_NAME}"
    PUBLIC NKL_VERSION_MAJOR=0
    PUBLIC NKL_VERSION_MINOR=0
    PUBLIC NKL_VERSION_PATCH=0
    )

target_link_libraries(${PROJECT_NAME}
    PRIVATE nkl_core
    )
