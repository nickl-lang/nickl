set(CMAKE_SYSTEM_NAME Linux)

set(TOOLCHAIN_DIR /opt/toolchain)
set(TOOLCHAIN_PREFIX x86_64-pc-linux-gnu)

set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_DIR}/bin/gcc)

set(CMAKE_LINKER ${TOOLCHAIN_DIR}/bin/ld)
set(CMAKE_AR ${TOOLCHAIN_DIR}/bin/ar)
set(CMAKE_NM ${TOOLCHAIN_DIR}/bin/nm)
set(CMAKE_OBJCOPY ${TOOLCHAIN_DIR}/bin/objcopy)
set(CMAKE_OBJDUMP ${TOOLCHAIN_DIR}/bin/objdump)
set(CMAKE_RANLIB ${TOOLCHAIN_DIR}/bin/ranlib)
set(CMAKE_STRIP ${TOOLCHAIN_DIR}/bin/strip)

set(CMAKE_FIND_ROOT_PATH "${TOOLCHAIN_DIR}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
