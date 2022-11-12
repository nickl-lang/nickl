set(PLATFORM_PREFIX /opt/mingw64)
set(PLATFORM_INCLUDE_DIR ${PLATFORM_PREFIX}/include)

add_library(Ffi SHARED IMPORTED)
set_target_properties(Ffi PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/bin/libffi-8.dll
    IMPORTED_IMPLIB ${PLATFORM_PREFIX}/lib/libffi.dll.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

add_library(Dl STATIC IMPORTED)
set_target_properties(Dl PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/lib/libdl.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

add_library(Gtest SHARED IMPORTED)
set_target_properties(Gtest PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/bin/libgtest.dll
    IMPORTED_IMPLIB ${PLATFORM_PREFIX}/lib/libgtest.dll.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

add_library(Gtest_Main SHARED IMPORTED)
set_target_properties(Gtest_Main PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/bin/libgtest_main.dll
    IMPORTED_IMPLIB ${PLATFORM_PREFIX}/lib/libgtest_main.dll.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

get_target_property(Ffi_LIBRARY Ffi IMPORTED_LOCATION)
get_target_property(Gtest_LIBRARY Gtest IMPORTED_LOCATION)
get_target_property(Gtest_Main_LIBRARY Gtest_Main IMPORTED_LOCATION)

install(
    FILES
        ${Ffi_LIBRARY}
        ${Gtest_LIBRARY}
        ${Gtest_Main_LIBRARY}
    DESTINATION bin
    )

set(PLATFORM_FLAGS "-static-libstdc++ -static-libgcc -static")
