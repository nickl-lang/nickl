set(LIB nkl_core2)

add_library(${LIB} SHARED
    src/ast_parser.c
    src/hash_trees.c
    src/ir_parser.c
    src/lexer.c
    src/nickl.c
    src/nickl_impl.c
    )

target_include_directories(${LIB}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(${LIB}
    PUBLIC nkl_common
    PUBLIC ntk
    PUBLIC nkb2
    )

include(GetTargetTriple)
get_target_triple(
    ARCH   TARGET_ARCH
    VENDOR TARGET_VENDOR
    SYS    TARGET_SYS
    ABI    TARGET_ABI
    )
target_compile_definitions(${LIB}
    PRIVATE HOST_TARGET_ARCH="${TARGET_ARCH}"
    PRIVATE HOST_TARGET_VENDOR="${TARGET_VENDOR}"
    PRIVATE HOST_TARGET_SYS="${TARGET_SYS}"
    PRIVATE HOST_TARGET_ABI="${TARGET_ABI}"
    )

if(NOT ENABLE_ASAN)
    if(APPLE)
        target_link_options(${LIB}
            PUBLIC "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/etc/exported-symbols.txt"
            )
    else()
        target_link_options(${LIB}
            PUBLIC "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/etc/exports.map"
            )
    endif()
endif()

install(TARGETS ${LIB}
    RUNTIME DESTINATION ${SYSTEM_INSTALL_DIR}
    )

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(test)
endif()
