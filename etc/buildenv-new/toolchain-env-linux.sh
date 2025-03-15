#!/bin/sh

export TOOLCHAIN_DIR=/opt/toolchain
export TOOLCHAIN_PREFIX=x86_64-pc-linux-gnu

export PATH="$TOOLCHAIN_DIR/bin:$PATH"

export CC="$TOOLCHAIN_DIR/bin/gcc"
export CXX="$TOOLCHAIN_DIR/bin/g++"
export LD="$TOOLCHAIN_DIR/bin/ld"
export AR="$TOOLCHAIN_DIR/bin/ar"
export NM="$TOOLCHAIN_DIR/bin/nm"
export OBJCOPY="$TOOLCHAIN_DIR/bin/objcopy"
export OBJDUMP="$TOOLCHAIN_DIR/bin/objdump"
export RANLIB="$TOOLCHAIN_DIR/bin/ranlib"
export STRIP="$TOOLCHAIN_DIR/bin/strip"

export C_INCLUDE_PATH="$TOOLCHAIN_DIR/include"
export CPLUS_INCLUDE_PATH="$TOOLCHAIN_DIR/include"
export LIBRARY_PATH="$TOOLCHAIN_DIR/lib"
export LD_LIBRARY_PATH="$TOOLCHAIN_DIR/lib"

export CFLAGS="-I$TOOLCHAIN_DIR/include"
export CXXFLAGS="-I$TOOLCHAIN_DIR/include"
export LDFLAGS="-L$TOOLCHAIN_DIR/lib"
