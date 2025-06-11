set(LIB nkb2)

add_library(${LIB}
    src/common.c
    src/ir.c
    src/linker.c
    src/llvm_adapter.c
    src/llvm_adapter.cpp
    src/llvm_emitter.c
    src/llvm_stream.c
    src/types.c
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
