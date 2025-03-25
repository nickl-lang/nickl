#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

print_usage() {
  echo >&2 "Usage: $0 [OPTIONS] [TARGET...]"
}

PARSED=$(ARGPARSE_SIMPLE=1 "$DIR/etc/utils/argparse.sh" "$0" \
  '-h,--help:-s,--system,=:-m,--machine,=:-n,--native:-d,--debug:-t,--test:-l,--logs:-p,--prof:-a,--asan' "$@") || {
  print_usage
  echo >&2 "Use --help for more info"
  exit 1
}
eval "$PARSED"
eval set -- "$__POS_ARGS"

SYSTEMS='linux windows darwin'
MACHINES='x86_64 arm64'

[ "$HELP" = 1 ] && {
  print_usage
  echo >&2 "Options:"
  echo >&2 "  -h, --help            Show this message"
  echo >&2 "  -s, --system SYSTEM   Target system, possible values: $SYSTEMS"
  echo >&2 "  -m, --machine MACHINE Target machine, possible values: $MACHINES"
  echo >&2 "  -n, --native          Enable native build, i.e. without docker"
  echo >&2 "  -d, --debug           Build with debug information"
  echo >&2 "  -t, --test            Build tests"
  echo >&2 "  -l, --logs            Enable logging support"
  echo >&2 "  -p, --prof            Enable profiling support"
  echo >&2 "  -a, --asan            Enable address sanitizer"
  echo >&2 ""
  exit
}

[ -n "${NATIVE+x}" ] && {
  [ -n "${SYSTEM+x}" ] && {
    echo >&2 "ERROR: Cannot use --system and --native together"
    exit 1
  }
  [ -n "${MACHINE+x}" ] && {
    echo >&2 "ERROR: Cannot use --machine and --native together"
    exit 1
  }
}

[ -z ${SYSTEM+x} ] && {
  SYSTEM=$(cmake -P "$DIR/cmake/GetSystemName.cmake" | tr '[:upper:]' '[:lower:]')
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
  MACHINE=$(cmake -P "$DIR/cmake/GetMachineName.cmake")
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

BUILD_SUFFIX="$SYSTEM-$MACHINE-$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')"
[ -n "${NATIVE+x}" ] && BUILD_SUFFIX="$BUILD_SUFFIX-native"
BIN_DIR="$DIR/out/build-$BUILD_SUFFIX"

FORCE_CONF=0

BUILD_TESTS=$(cmake -L -N -B "$BIN_DIR" | grep ^BUILD_TESTS | cut -d '=' -f2)
[ "$TEST" = 1 ] && {
  [ "$BUILD_TESTS" != 'ON' ] && {
    FORCE_CONF=1
    EXTRA_CMAKE_ARGS="$EXTRA_CMAKE_ARGS -DBUILD_TESTS=ON"
    BUILD_TESTS='ON'
  }
}

[ "$LOGS" = 1 ] && {
  CUR=$(cmake -L -N -B "$BIN_DIR" | grep ^ENABLE_LOGGING | cut -d '=' -f2)
  [ "$CUR" != 'ON' ] && { FORCE_CONF=1; EXTRA_CMAKE_ARGS="$EXTRA_CMAKE_ARGS -DENABLE_LOGGING=ON"; }
}

[ "$PROF" = 1 ] && {
  CUR=$(cmake -L -N -B "$BIN_DIR" | grep ^ENABLE_PROFILING | cut -d '=' -f2)
  [ "$CUR" != 'ON' ] && { FORCE_CONF=1; EXTRA_CMAKE_ARGS="$EXTRA_CMAKE_ARGS -DENABLE_PROFILING=ON"; }
}

[ "$ASAN" = 1 ] && {
  CUR=$(cmake -L -N -B "$BIN_DIR" | grep ^ENABLE_ASAN | cut -d '=' -f2)
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

if [ ! -f "$BIN_DIR/$MAKEFILE" ] ||
   [ ! -f "$BIN_DIR/CMakeCache.txt" ] ||
   [ "$FORCE_CONF" = 1 ]; then
  cmake -S "$DIR" -B "$BIN_DIR" \
    -DCMAKE_INSTALL_PREFIX="$DIR/out/install-$BUILD_SUFFIX" \
    -DDEPLOY_PREFIX="$DIR/out/deploy-$BUILD_SUFFIX" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
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
