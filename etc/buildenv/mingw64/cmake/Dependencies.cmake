set(PLATFORM_PREFIX /opt/mingw64)
set(PLATFORM_INCLUDE_DIR ${PLATFORM_PREFIX}/include)

add_library(Ffi STATIC IMPORTED)
set_target_properties(Ffi PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/lib/libffi.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

add_library(Dl STATIC IMPORTED)
set_target_properties(Dl PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/lib/libdl.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

set(PLATFORM_CXX_FLAGS "-static-libstdc++ -static-libgcc -fstack-protector -static")
