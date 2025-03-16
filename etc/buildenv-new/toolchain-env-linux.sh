#!/bin/sh

PREFIX="/opt/toolchain"

export PATH="$PREFIX/bin:$PATH"

export CC="$PREFIX/bin/gcc"
export CXX="$PREFIX/bin/g++"
export LD="$PREFIX/bin/ld"
export AR="$PREFIX/bin/ar"
export NM="$PREFIX/bin/nm"
export OBJCOPY="$PREFIX/bin/objcopy"
export OBJDUMP="$PREFIX/bin/objdump"
export RANLIB="$PREFIX/bin/ranlib"
export STRIP="$PREFIX/bin/strip"

export C_INCLUDE_PATH="$PREFIX/include"
export CPLUS_INCLUDE_PATH="$PREFIX/include"
export LIBRARY_PATH="$PREFIX/lib"
export LD_LIBRARY_PATH="$PREFIX/lib"

export CFLAGS="-I$PREFIX/include"
export CXXFLAGS="-I$PREFIX/include"
export LDFLAGS="-L$PREFIX/lib"
