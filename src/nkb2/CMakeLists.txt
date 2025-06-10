set(LIB nkb2)

add_library(${LIB}
    src/common.c
    src/emit_llvm.c
    src/ir.c
    src/linker.c
    src/llvm_stream.c
    src/types.c
    src/llvm.cpp
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC ntk
    )

include(LinkLLVM)
target_link_llvm(
    TARGET ${LIB}
    COMPONENTS
        Core
        ExecutionEngine
        IRReader
        Linker
        OrcJIT
        Passes
        Support
        Target
        X86CodeGen
        X86Desc
    )
