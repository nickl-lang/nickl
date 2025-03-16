#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

PROJ_ROOT="$DIR"

# if [ -f /.dockerenv ]; then
# else
#   "$PROJ_ROOT"/etc/buildenv/run-docker.sh $DIR/build.sh "$@"
# fi

if [ -z ${SYSTEM+x} ]; then
  SYSTEM=$(cmake -P "$PROJ_ROOT/cmake/GetSystemName.cmake")
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

# # TODO: Move this logic somewhere
# SYSTEM_ROOT="$PROJ_ROOT"/etc/buildenv/$SYSTEM
# if [ -f "$SYSTEM_ROOT"/scripts/prepare_buildenv.sh ]; then
#   "$SYSTEM_ROOT"/scripts/prepare_buildenv.sh
# fi

SYSTEM_SUFFIX=$(echo "$SYSTEM-$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')
BIN_DIR="$PROJ_ROOT/out/build-$SYSTEM_SUFFIX"
mkdir -p "$BIN_DIR"

if [ ! -f "$BIN_DIR/$MAKEFILE" ] ||
   [ ! -f "$BIN_DIR/CMakeCache.txt" ]; then
  cmake -S "$PROJ_ROOT" -B "$BIN_DIR" \
    -DCMAKE_INSTALL_PREFIX="$PROJ_ROOT/out/install-$SYSTEM_SUFFIX" \
    -DDEPLOY_PREFIX="$PROJ_ROOT/out/deploy-$SYSTEM_SUFFIX" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    ${EXTRA_CMAKE_ARGS:+$EXTRA_CMAKE_ARGS}
fi

NPROC=$(nproc)
export CTEST_PARALLEL_LEVEL="$NPROC"

$MAKE -C "$BIN_DIR" -j"$NPROC" "$@"
