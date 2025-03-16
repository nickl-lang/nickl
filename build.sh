#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

PROJECT_DIR="$DIR"

if [ -f /.dockerenv ]; then
  if [ -z ${SYSTEM+x} ]; then
    SYSTEM=$(cmake -P "$PROJECT_DIR/cmake/GetSystemName.cmake")
  fi

  if [ -z ${BUILD_TYPE+x} ]; then
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
      $EXTRA_CMAKE_ARGS
  fi

  NPROC=$(nproc)
  export CTEST_PARALLEL_LEVEL="$NPROC"

  $MAKE -C "$BIN_DIR" -j"$NPROC" "$@"
else
  "$PROJECT_DIR"/etc/buildenv/run-docker.sh "$DIR/build.sh" "$@"
fi
