def_test(GROUP lang NAME ast LINK ${LIB} nkl_core_test_utils)
def_test(GROUP lang NAME lexer LINK ${LIB})
def_test(GROUP lang NAME parser LINK ${LIB} nkl_core_test_utils)
def_test(GROUP lang NAME value LINK ${LIB})
