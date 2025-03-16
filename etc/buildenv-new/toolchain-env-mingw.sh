#!/bin/sh

PREFIX="/opt/toolchain"
TARGET="x86_64-w64-mingw32"

export PATH="$PREFIX/bin:$PATH"

export CC="$PREFIX/bin/${TARGET}-gcc"
export CXX="$PREFIX/bin/${TARGET}-g++"
export LD="$PREFIX/bin/${TARGET}-ld"
export AR="$PREFIX/bin/${TARGET}-ar"
export NM="$PREFIX/bin/${TARGET}-nm"
export OBJCOPY="$PREFIX/bin/${TARGET}-objcopy"
export OBJDUMP="$PREFIX/bin/${TARGET}-objdump"
export RANLIB="$PREFIX/bin/${TARGET}-ranlib"
export STRIP="$PREFIX/bin/${TARGET}-strip"

export C_INCLUDE_PATH="$PREFIX/include"
export CPLUS_INCLUDE_PATH="$PREFIX/include"
export LIBRARY_PATH="$PREFIX/lib"
export LD_LIBRARY_PATH="$PREFIX/lib"

export CFLAGS="-I$PREFIX/include"
export CXXFLAGS="-I$PREFIX/include"
export LDFLAGS="-L$PREFIX/lib"
