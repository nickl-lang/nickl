set(LIB blake3)

enable_language(ASM)

add_library(${LIB} STATIC
    src/blake3.c
    src/blake3_dispatch.c
    src/blake3_portable.c
    )

if(WIN32)
    target_sources(${LIB} PRIVATE
        src/blake3_avx2_x86-64_windows_gnu.S
        src/blake3_sse2_x86-64_windows_gnu.S
        src/blake3_sse41_x86-64_windows_gnu.S
        src/blake3_avx512_x86-64_windows_gnu.S
    )
else()
    target_sources(${LIB} PRIVATE
        src/blake3_avx2_x86-64_unix.S
        src/blake3_sse2_x86-64_unix.S
        src/blake3_sse41_x86-64_unix.S
        src/blake3_avx512_x86-64_unix.S
    )
endif()

target_include_directories(${LIB} SYSTEM
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
