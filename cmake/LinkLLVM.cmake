function(target_link_llvm)
    set(options)
    set(oneValueArgs TARGET)
    set(multiValueArgs COMPONENTS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "TARGET argument is required")
    endif()

    if(NOT ARG_COMPONENTS)
        message(FATAL_ERROR "COMPONENTS argument is required")
    endif()

    llvm_map_components_to_libnames(LLVM_LIBS ${ARG_COMPONENTS})

    set(MISSING_LIBS "")
    set(FOUND_ALL_STATIC_LIBS TRUE)

    foreach(LIB IN LISTS LLVM_LIBS)
        if(NOT EXISTS "${LLVM_LIBRARY_DIRS}/lib${LIB}.a")
            set(FOUND_ALL_STATIC_LIBS FALSE)
            list(APPEND MISSING_LIBS "lib${LIB}.a")
        endif()
    endforeach()

    if(FOUND_ALL_STATIC_LIBS)
        target_link_libraries(${ARG_TARGET}
            PRIVATE ${LLVM_LIBS}
            )
    else()
        list(JOIN MISSING_LIBS ", " MISSING_LIBS_STR)
        message(WARNING "Linking LLVM dynamically, \
            following static libs are missing: [ ${MISSING_LIBS_STR} ]")

        target_link_libraries(${ARG_TARGET}
            PRIVATE LLVM
            )
    endif()

    target_include_directories(${ARG_TARGET} SYSTEM
        PRIVATE ${LLVM_INCLUDE_DIRS}
    )

    separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
    target_compile_definitions(${ARG_TARGET}
        PRIVATE ${LLVM_DEFINITIONS_LIST}
        )
endfunction()
