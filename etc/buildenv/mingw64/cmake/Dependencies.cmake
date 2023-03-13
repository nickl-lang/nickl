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

set(OUT_DIR ${CMAKE_BINARY_DIR}/deps)

file(
    COPY
        /usr/lib/gcc/${TOOLCHAIN_PREFIX}/${TOOLCHAIN_VERSISON}/libgcc_s_seh-1.dll
        /usr/lib/gcc/${TOOLCHAIN_PREFIX}/${TOOLCHAIN_VERSISON}/libssp-0.dll
        /usr/lib/gcc/${TOOLCHAIN_PREFIX}/${TOOLCHAIN_VERSISON}/libstdc++-6.dll
        /usr/${TOOLCHAIN_PREFIX}/lib/libwinpthread-1.dll
    DESTINATION ${OUT_DIR}
    )

add_custom_command(
    OUTPUT ${OUT_DIR}/stripped
    COMMAND ${CMAKE_STRIP} ${OUT_DIR}/libgcc_s_seh-1.dll
    COMMAND ${CMAKE_STRIP} ${OUT_DIR}/libssp-0.dll
    COMMAND ${CMAKE_STRIP} ${OUT_DIR}/libstdc++-6.dll
    COMMAND ${CMAKE_STRIP} ${OUT_DIR}/libwinpthread-1.dll
    COMMAND ${CMAKE_COMMAND} -E touch ${OUT_DIR}/stripped
    COMMENT "Stripping MinGW64 dependencies"
    VERBATIM)

add_custom_target(strip_mingw64_deps ALL
    DEPENDS ${CMAKE_STRIP} ${OUT_DIR}/libgcc_s_seh-1.dll
    DEPENDS ${CMAKE_STRIP} ${OUT_DIR}/libssp-0.dll
    DEPENDS ${CMAKE_STRIP} ${OUT_DIR}/libstdc++-6.dll
    DEPENDS ${CMAKE_STRIP} ${OUT_DIR}/libwinpthread-1.dll
    DEPENDS ${OUT_DIR}/stripped
    )

install(
    PROGRAMS
        ${OUT_DIR}/libgcc_s_seh-1.dll
        ${OUT_DIR}/libssp-0.dll
        ${OUT_DIR}/libstdc++-6.dll
        ${OUT_DIR}/libwinpthread-1.dll
    DESTINATION bin
    )

set(PLATFORM_CXX_FLAGS "-fstack-protector")
