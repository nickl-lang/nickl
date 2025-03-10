#!/bin/sh

PREFIX=x86_64-w64-mingw32

TOOLS="/opt/toolchain"

export PATH="$TOOLS/bin:$PATH"

export CC="$TOOLS/bin/$PREFIX-gcc"
export CXX="$TOOLS/bin/$PREFIX-g++"
export LD="$TOOLS/bin/$PREFIX-ld"
export AR="$TOOLS/bin/$PREFIX-ar"
export NM="$TOOLS/bin/$PREFIX-nm"
export OBJCOPY="$TOOLS/bin/$PREFIX-objcopy"
export OBJDUMP="$TOOLS/bin/$PREFIX-objdump"
export RANLIB="$TOOLS/bin/$PREFIX-ranlib"
export STRIP="$TOOLS/bin/$PREFIX-strip"

export C_INCLUDE_PATH="$TOOLS/include:$C_INCLUDE_PATH"
export CPLUS_INCLUDE_PATH="$TOOLS/include:$CPLUS_INCLUDE_PATH"
export LIBRARY_PATH="$TOOLS/lib:$TOOLS/lib64:$LIBRARY_PATH"
export LD_LIBRARY_PATH="$TOOLS/lib:$TOOLS/lib64:$LD_LIBRARY_PATH"

export CFLAGS="$CFLAGS -I$TOOLS/include"
export CXXFLAGS="$CXXFLAGS -I$TOOLS/include"
export LDFLAGS="$LDFLAGS -L$TOOLS/lib -L$TOOLS/lib64"
