set(PLATFORM_PREFIX /usr/lib)
set(PLATFORM_INCLUDE_DIR /usr/include)

add_library(Ffi SHARED IMPORTED)
set_target_properties(Ffi PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/libffi.so
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

add_library(Dl STATIC IMPORTED)
set_target_properties(Dl PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/libdl.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

add_library(Gtest SHARED IMPORTED)
set_target_properties(Gtest PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/libgtest.so
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

add_library(Gtest_Main SHARED IMPORTED)
set_target_properties(Gtest_Main PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/libgtest_main.so
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

get_target_property(Ffi_LIBRARY Ffi IMPORTED_LOCATION)
get_target_property(Gtest_LIBRARY Gtest IMPORTED_LOCATION)
get_target_property(Gtest_Main_LIBRARY Gtest_Main IMPORTED_LOCATION)

file(GLOB Ffi_INSTALL_FILES ${Ffi_LIBRARY}*)
file(GLOB Gtest_INSTALL_FILES ${Gtest_LIBRARY}*)
file(GLOB Gtest_Main_INSTALL_FILES ${Gtest_Main_LIBRARY}*)

install(
    FILES
        ${Ffi_INSTALL_FILES}
        ${Gtest_INSTALL_FILES}
        ${Gtest_Main_INSTALL_FILES}
    DESTINATION bin
    )

set(PLATFORM_FLAGS "-static-libstdc++ -static-libgcc")
