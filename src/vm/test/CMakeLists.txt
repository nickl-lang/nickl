set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic") # TODO For dlsym

def_test(GROUP vm NAME ir LINK ${LIB})
