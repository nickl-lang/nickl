def_test(GROUP vm NAME ir LINK ${LIB} TARGET IR_TEST)

target_link_options(${IR_TEST}
    PRIVATE -export-dynamic # for dl
    )
