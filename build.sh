#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

print_usage() {
  echo >&2 "Usage: $0 [OPTIONS] [TARGET...]"
}

SYSTEMS='linux windows darwin'
MACHINES='x86_64 arm64'

OPTIONS="
  -h, --help               : Show this message
  -s, --system SYSTEM      : Target system, possible values: $SYSTEMS
  -m, --machine MACHINE    : Target machine, possible values: $MACHINES
  -n, --native             : Enable native build, i.e. without docker
  -d, --debug              : Build with debug information
  -t, --test               : Build tests
  -l, --logs               : Enable logging support
  -p, --prof               : Enable profiling support
  -a, --asan               : Enable address sanitizer
  -k, --osx-sdk PATH       : Path to OSX SDK
"

PARSED=$(ARGPARSE_SIMPLE=1 "$DIR/etc/utils/argparse.sh" "$0" "$OPTIONS" "$@") || {
  print_usage
  echo >&2 "Use --help for more info"
  exit 1
}
eval "$PARSED"
eval set -- "$__POS_ARGS"

[ "$HELP" = 1 ] && {
  print_usage
  echo >&2 "Options:$OPTIONS"
  exit
}

[ -z ${SYSTEM+x} ] && {
  SYSTEM=$("$DIR/etc/utils/get_system_name.sh" | tr '[:upper:]' '[:lower:]')
}

case "$SYSTEM" in
  linux);;
  windows);;
  darwin);;
  *)
    echo >&2 "ERROR: Invalid system '$SYSTEM', possible values: $SYSTEMS"
    exit 1
    ;;
esac

[ -z ${MACHINE+x} ] && {
  MACHINE=$("$DIR/etc/utils/get_machine_name.sh")
}

case "$MACHINE" in
  x86_64);;
  arm64);;
  *)
    echo >&2 "ERROR: Invalid machine '$MACHINE', possible values: $MACHINES"
    exit 1
    ;;
esac

[ "$DEBUG" = 1 ] && BUILD_TYPE='Debug'
[ -z ${BUILD_TYPE+x} ] && BUILD_TYPE='Release'

BUILD="$SYSTEM-$MACHINE-$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')"
[ -n "${NATIVE+x}" ] && BUILD="$BUILD-native"
BIN_DIR="$DIR/out/build/$BUILD"

FORCE_CONF=0

BUILD_TESTS=$("$DIR/etc/utils/get_cmake_cache_var.sh" "$BIN_DIR" BUILD_TESTS)
[ "$TEST" = 1 ] && {
  [ "$BUILD_TESTS" != 'ON' ] && {
    FORCE_CONF=1
    EXTRA_CMAKE_ARGS="$EXTRA_CMAKE_ARGS -DBUILD_TESTS=ON"
    BUILD_TESTS='ON'
  }
}

[ "$LOGS" = 1 ] && {
  CUR=$("$DIR/etc/utils/get_cmake_cache_var.sh" "$BIN_DIR" ENABLE_LOGGING)
  [ "$CUR" != 'ON' ] && { FORCE_CONF=1; EXTRA_CMAKE_ARGS="$EXTRA_CMAKE_ARGS -DENABLE_LOGGING=ON"; }
}

[ "$PROF" = 1 ] && {
  CUR=$("$DIR/etc/utils/get_cmake_cache_var.sh" "$BIN_DIR" ENABLE_PROFILING)
  [ "$CUR" != 'ON' ] && { FORCE_CONF=1; EXTRA_CMAKE_ARGS="$EXTRA_CMAKE_ARGS -DENABLE_PROFILING=ON"; }
}

[ "$ASAN" = 1 ] && {
  CUR=$("$DIR/etc/utils/get_cmake_cache_var.sh" "$BIN_DIR" ENABLE_ASAN)
  [ "$CUR" != 'ON' ] && { FORCE_CONF=1; EXTRA_CMAKE_ARGS="$EXTRA_CMAKE_ARGS -DENABLE_ASAN=ON"; }
}

if [ -z ${MAKE+x} ]; then
  if [ -f "$BIN_DIR/Makefile" ]; then
    MAKE='make'
  elif command -v ninja >/dev/null 2>&1; then
    MAKE='ninja'
  else
    MAKE='make'
  fi
fi

if [ "$NATIVE" != 1 ] && [ ! -f /.dockerenv ]; then
  if [ "$BUILD_TESTS" = 'ON' ]; then
    IMAGE="$SYSTEM-$MACHINE-dev"
  else
    IMAGE="$SYSTEM-$MACHINE"
  fi

  [ "$SYSTEM" = 'darwin' ] && {
    [ -z "${OSX_SDK+x}" ] && {
      echo >&2 "ERROR: Provide path to OSX SDK with --osx-sdk"
      exit 1
    }
    export EXTRA_DOCKER_OPTS="$EXTRA_DOCKER_OPTS -v $OSX_SDK:/opt/toolchain/SDK"
  }

  "$DIR/etc/buildenv/run-docker.sh" -i "$IMAGE" -- "$DIR/build.sh" $__ALL_ARGS
  exit
fi

case "$MAKE" in
  ninja)
    MAKEFILE='build.ninja'
    EXTRA_CMAKE_ARGS="$EXTRA_CMAKE_ARGS -GNinja"
    ;;
  make)
    MAKEFILE='Makefile'
    ;;
  *)
    echo >&2 "ERROR: Unknown make tool MAKE=$MAKE"
    exit 1
    ;;
esac

command -v cmake >/dev/null 2>&1 || {
  echo >&2 'ERROR: `cmake` is not installed'
  exit 1
}

if [ ! -f "$BIN_DIR/$MAKEFILE" ] ||
   [ ! -f "$BIN_DIR/CMakeCache.txt" ] ||
   [ "$FORCE_CONF" = 1 ]; then
  cmake -S "$DIR" -B "$BIN_DIR" \
    -DCMAKE_INSTALL_PREFIX="$DIR/out/install/$BUILD" \
    -DDEPLOY_PREFIX="$DIR/out/deploy" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DTARGET_MACHINE="$MACHINE" \
    $EXTRA_CMAKE_ARGS
fi

numcores() {
  case $(uname -s) in
    Linux) nproc ;;
    Darwin) sysctl -n hw.logicalcpu 2>/dev/null ;;
    *) echo 1 ;;
  esac
}

[ -z ${NPROC+x} ] && NPROC=$(numcores)
[ -z ${CTEST_PARALLEL_LEVEL+x} ] && {
  CTEST_PARALLEL_LEVEL="$NPROC"
  export CTEST_PARALLEL_LEVEL
}

$MAKE -C "$BIN_DIR" -j"$NPROC" "$@"
