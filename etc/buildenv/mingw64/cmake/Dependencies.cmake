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

add_library(Shlwapi STATIC IMPORTED)
set_target_properties(Shlwapi PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/lib/libshlwapi.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

set(SYSTEM_DEPS
    # /usr/lib/gcc/${TOOLCHAIN_PREFIX}/${TOOLCHAIN_VERSISON}/libgcc_s_seh-1.dll
    # /usr/lib/gcc/${TOOLCHAIN_PREFIX}/${TOOLCHAIN_VERSISON}/libssp-0.dll
    # /usr/lib/gcc/${TOOLCHAIN_PREFIX}/${TOOLCHAIN_VERSISON}/libstdc++-6.dll
    # /usr/${TOOLCHAIN_PREFIX}/lib/libwinpthread-1.dll
    )

set(PLATFORM_CXX_FLAGS "-fstack-protector -static-libstdc++")
