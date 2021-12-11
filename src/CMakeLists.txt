add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(lang)
add_subdirectory(vm)

add_executable(${PROJECT_NAME}
    main.cpp
    )
