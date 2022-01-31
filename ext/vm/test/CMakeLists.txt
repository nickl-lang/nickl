add_library(ir_utils
    utils/ir_utils.cpp
    )

target_link_libraries(ir_utils
    PUBLIC ${LIB}
    )

def_test(GROUP vm NAME value LINK ${LIB})

def_test(GROUP vm NAME ir LINK ${LIB} ir_utils)
def_test(GROUP vm NAME vm LINK ${LIB} ir_utils)
def_test(GROUP vm NAME interp LINK ${LIB} ir_utils)
