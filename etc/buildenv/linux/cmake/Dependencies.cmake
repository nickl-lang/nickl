set(PLATFORM_PREFIX /usr/lib/x86_64-linux-gnu)
set(PLATFORM_INCLUDE_DIR /usr/include)

add_library(Ffi STATIC IMPORTED)
set_target_properties(Ffi PROPERTIES
    IMPORTED_LOCATION /opt/toolchain/lib64/libffi.a
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

add_library(Dl SHARED IMPORTED)
set_target_properties(Dl PROPERTIES
    IMPORTED_LOCATION ${PLATFORM_PREFIX}/libdl.so
    INTERFACE_INCLUDE_DIRECTORIES ${PLATFORM_INCLUDE_DIR}
    )

set(PLATFORM_CXX_FLAGS "-static-libstdc++ -static-libgcc")
