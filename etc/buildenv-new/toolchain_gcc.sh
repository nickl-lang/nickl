#!/bin/sh

TOOLS="/opt/toolchain"

export PATH="$TOOLS/bin:$PATH"

export CC="$TOOLS/bin/gcc"
export CXX="$TOOLS/bin/g++"
export LD="$TOOLS/bin/ld"
export AR="$TOOLS/bin/ar"
export NM="$TOOLS/bin/nm"
export OBJCOPY="$TOOLS/bin/objcopy"
export OBJDUMP="$TOOLS/bin/objdump"
export RANLIB="$TOOLS/bin/ranlib"
export STRIP="$TOOLS/bin/strip"

export C_INCLUDE_PATH="$TOOLS/include:$C_INCLUDE_PATH"
export CPLUS_INCLUDE_PATH="$TOOLS/include:$CPLUS_INCLUDE_PATH"
export LIBRARY_PATH="$TOOLS/lib:$TOOLS/lib64:$LIBRARY_PATH"
export LD_LIBRARY_PATH="$TOOLS/lib:$TOOLS/lib64:$LD_LIBRARY_PATH"

export CFLAGS="-I$TOOLS/include $CFLAGS"
export CXXFLAGS="-I$TOOLS/include $CXXFLAGS"
export LDFLAGS="-L$TOOLS/lib -L$TOOLS/lib64 $LDFLAGS"
