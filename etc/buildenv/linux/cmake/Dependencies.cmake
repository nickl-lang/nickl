set(PLATFORM_PREFIX /usr/lib/x86_64-linux-gnu)
set(PLATFORM_INCLUDE_DIR /usr/include)

add_library(Ffi SHARED IMPORTED)
set_target_properties(Ffi PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/libffi.so
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

add_library(Dl SHARED IMPORTED)
set_target_properties(Dl PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/libdl.so
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

add_library(Gtest STATIC IMPORTED)
set_target_properties(Gtest PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/libgtest.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

add_library(Gtest_Main SHARED IMPORTED)
set_target_properties(Gtest_Main PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/libgtest_main.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

get_target_property(Ffi_LIBRARY Ffi IMPORTED_LOCATION)

install(
    FILES
        ${Ffi_LIBRARY}
    DESTINATION bin
    )

set(PLATFORM_FLAGS "-static-libstdc++ -static-libgcc")

set(Threads_LIBRARY pthread)
