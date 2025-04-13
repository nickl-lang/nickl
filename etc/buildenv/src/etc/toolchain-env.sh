#!/bin/sh

if [ -z ${TARGET+x} ]; then
  echo >&2 "ERROR: Specify TARGET"
  exit 1
fi

PREFIX="/opt/toolchain"

export PATH="$PREFIX/bin:$PATH"

export CC="$PREFIX/bin/${TARGET}-cc"
export CXX="$PREFIX/bin/${TARGET}-c++"
export LD="$PREFIX/bin/${TARGET}-ld"
export AR="$PREFIX/bin/${TARGET}-ar"
export NM="$PREFIX/bin/${TARGET}-nm"
export OBJCOPY="$PREFIX/bin/${TARGET}-objcopy"
export OBJDUMP="$PREFIX/bin/${TARGET}-objdump"
export RANLIB="$PREFIX/bin/${TARGET}-ranlib"
export STRIP="$PREFIX/bin/${TARGET}-strip"
export INSALL_NAME_TOOL="$PREFIX/bin/${TARGET}-install_name_tool"

export C_INCLUDE_PATH="$PREFIX/include"
export CPLUS_INCLUDE_PATH="$PREFIX/include"
export LIBRARY_PATH="$PREFIX/lib"
export LD_LIBRARY_PATH="$PREFIX/lib"

export CFLAGS="-I$PREFIX/include"
export CXXFLAGS="-I$PREFIX/include"
export LDFLAGS="-L$PREFIX/lib"
