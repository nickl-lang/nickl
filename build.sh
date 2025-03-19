#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

print_usage() {
  echo >&2 "Usage: $0 [OPTIONS] [TARGET...]"
}

PARSED=$(ARGPARSE_SIMPLE=1 "$DIR/etc/utils/argparse.sh" "$0" \
  '-h,--help:-n,--native:-t,--test:-d,--debug:-s,--system,=' "$@") || {
  print_usage
  echo >&2 "Use --help for more info"
  exit 1
}
eval "$PARSED"
eval set -- "$__POS_ARGS"

[ "$HELP" = 1 ] && {
  print_usage
  echo >&2 "Options:"
  echo >&2 "  -h, --help          Show this message"
  echo >&2 "  -n, --native        Enable native build, i.e. without docker"
  echo >&2 "  -t, --test          Build tests"
  echo >&2 "  -d, --debug         Build with debug information"
  echo >&2 "  -s, --system SYSTEM Build target, possible values: linux windows"
  echo >&2 ""
  exit
}

if [ "$DEBUG" = 1 ]; then
  BUILD_TYPE='Debug'
else
  BUILD_TYPE='Release'
fi

[ -z ${SYSTEM+x} ] && {
  SYSTEM=$(cmake -P "$DIR/cmake/GetSystemName.cmake" | tr '[:upper:]' '[:lower:]')
}

case "$SYSTEM" in
  linux);;
  windows);;
  *)
    echo >&2 "ERROR: Invalid system '$SYSTEM', possible values are: linux windows"
    exit 1
    ;;
esac

SYSTEM_SUFFIX=$(echo "$SYSTEM-$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')
BIN_DIR="$DIR/out/build-$SYSTEM_SUFFIX"

BUILD_TESTS_INIT=$(cmake -L -N -B "$BIN_DIR" | grep ^BUILD_TESTS | cut -d '=' -f2)
[ -z ${BUILD_TESTS+x} ] && BUILD_TESTS="$BUILD_TESTS_INIT"

[ "$TEST" = 1 ] && BUILD_TESTS='ON'

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
    IMAGE="$SYSTEM-dev"
  else
    IMAGE="$SYSTEM"
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
   [ "$BUILD_TESTS_INIT" != "$BUILD_TESTS" ]; then
  cmake -S "$DIR" -B "$BIN_DIR" \
    -DCMAKE_INSTALL_PREFIX="$DIR/out/install-$SYSTEM_SUFFIX" \
    -DDEPLOY_PREFIX="$DIR/out/deploy-$SYSTEM_SUFFIX" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_TESTS="$BUILD_TESTS" \
    $EXTRA_CMAKE_ARGS
fi

[ -z ${NPROC+x} ] && NPROC=$(nproc)
[ -z ${CTEST_PARALLEL_LEVEL+x} ] && {
  CTEST_PARALLEL_LEVEL="$NPROC"
  export CTEST_PARALLEL_LEVEL
}

$MAKE -C "$BIN_DIR" -j"$NPROC" "$@"
