def_test(GROUP common NAME allocator LINK nk_common)
def_test(GROUP common NAME id LINK nk_common)
def_test(GROUP common NAME string_builder LINK nk_common)

if(ENABLE_LOGGING)
    def_test(GROUP common NAME logger LINK nk_common)
endif()
