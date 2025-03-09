set(PLATFORM_PREFIX /opt/toolchain)
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

add_library(Shlwapi STATIC IMPORTED)
set_target_properties(Shlwapi PROPERTIES
    IMPORTED_LOCATION /opt/toolchain/x86_64-w64-mingw32/lib/libshlwapi.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

add_library(Winpthread STATIC IMPORTED)
set_target_properties(Winpthread PROPERTIES
    IMPORTED_LOCATION /opt/toolchain/x86_64-w64-mingw32/lib/libwinpthread.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

set(PLATFORM_CXX_FLAGS "-static-libstdc++ -static-libgcc")
