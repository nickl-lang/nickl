def_test(GROUP common NAME allocator LINK ${LIB})
def_test(GROUP common NAME id LINK ${LIB})
def_test(GROUP common NAME string_builder LINK ${LIB})

if(ENABLE_LOGGING)
    def_test(GROUP common NAME logger LINK ${LIB})
endif()
