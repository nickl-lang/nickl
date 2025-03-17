#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

ORIGINAL_ARGS="$@"

print_usage() {
  echo >&2 "Usage: $0 [-h] [-n] [-t] [-d] [-s SYSTEM] [TARGET]"
}

print_help() {
  print_usage
  echo >&2 "Options:"
  echo >&2 "  -h, --help          Show this message"
  echo >&2 "  -n, --native        Enable native build, i.e. without docker"
  echo >&2 "  -t, --test          Build tests"
  echo >&2 "  -d, --debug         Build with debug information"
  echo >&2 "  -s, --system SYSTEM Build for SYSTEM"
}

NATIVE_BUILD=OFF
BUILD_TESTS=OFF
DEBUG_BUILD=OFF
EXTRA_ARGS=""
unset SYSTEM
while [ "$#" -gt 0 ]; do
  case "$1" in
    -h|--help)
      print_help
      exit 0
      ;;
    -n|--native)
      NATIVE_BUILD=ON
      shift
      ;;
    -t|--test)
      BUILD_TESTS=ON
      shift
      ;;
    -d|--debug)
      DEBUG_BUILD=ON
      shift
      ;;
    -s|--system)
      SYSTEM="$2"
      shift 2
      ;;
    *)
      EXTRA_ARGS="$EXTRA_ARGS $1"
      shift
      ;;
  esac
done

PROJECT_DIR="$DIR"

if [ -z ${SYSTEM+x} ]; then
  SYSTEM=$(cmake -P "$PROJECT_DIR/cmake/GetSystemName.cmake" | tr '[:upper:]' '[:lower:]')
fi

if [ "$BUILD_TESTS" = ON ]; then
  TARGET="$SYSTEM-dev"
else
  TARGET="$SYSTEM"
fi

export TARGET

if [ "$NATIVE_BUILD" = ON ] || [ -f /.dockerenv ]; then
  if [ "$DEBUG_BUILD" = ON ]; then
    BUILD_TYPE='Debug'
  else
    BUILD_TYPE='Release'
  fi

  if [ -z ${MAKE+x} ]; then
    if command -v ninja >/dev/null 2>&1; then
      MAKE='ninja'
    else
      MAKE='make'
    fi
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

  SYSTEM_SUFFIX=$(echo "$SYSTEM-$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')
  BIN_DIR="$PROJECT_DIR/out/build-$SYSTEM_SUFFIX"
  mkdir -p "$BIN_DIR"

  if [ ! -f "$BIN_DIR/$MAKEFILE" ] ||
     [ ! -f "$BIN_DIR/CMakeCache.txt" ]; then
    cmake -S "$PROJECT_DIR" -B "$BIN_DIR" \
      -DCMAKE_INSTALL_PREFIX="$PROJECT_DIR/out/install-$SYSTEM_SUFFIX" \
      -DDEPLOY_PREFIX="$PROJECT_DIR/out/deploy-$SYSTEM_SUFFIX" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DBUILD_TESTS="$BUILD_TESTS" \
      $EXTRA_CMAKE_ARGS
  fi

  NPROC=$(nproc)
  export CTEST_PARALLEL_LEVEL="$NPROC"

  $MAKE -C "$BIN_DIR" -j"$NPROC" $EXTRA_ARGS
else
  "$PROJECT_DIR/etc/buildenv/run-docker.sh" "$DIR/build.sh" $ORIGINAL_ARGS
fi
