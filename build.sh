#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

PROJ_ROOT=$DIR

# if [ -f /.dockerenv ]; then
  if [ -z ${PLATFORM+x} ]; then
      PLATFORM='linux'
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
      echo >&2 "ERROR: Unknown generator $MAKE"
      ;;
  esac

  # TODO: Move this logic somewhere
  PLATFORM_ROOT="$PROJ_ROOT"/etc/buildenv/$PLATFORM
  if [ -f "$PLATFORM_ROOT"/scripts/prepare_buildenv.sh ]; then
      "$PLATFORM_ROOT"/scripts/prepare_buildenv.sh
  fi

  PLATFORM_SUFFIX=$PLATFORM-$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')
  BIN_DIR="$PROJ_ROOT/out/build-$PLATFORM_SUFFIX"
  mkdir -p "$BIN_DIR"

  if [ ! -f "$BIN_DIR/$MAKEFILE" ] ||
     [ ! -f "$BIN_DIR/CMakeCache.txt" ]; then
    cmake -S "$PROJ_ROOT" -B "$BIN_DIR" \
      -DCMAKE_INSTALL_PREFIX="$PROJ_ROOT/out/install-$PLATFORM_SUFFIX" \
      -DDEPLOY_PREFIX="$PROJ_ROOT/out/deploy-$PLATFORM_SUFFIX" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      ${EXTRA_CMAKE_ARGS:+$EXTRA_CMAKE_ARGS}
  fi

  NPROC=$(nproc)
  export CTEST_PARALLEL_LEVEL="$NPROC"

  $MAKE -C "$BIN_DIR" -j"$NPROC" "$@"
# else
#   "$PROJ_ROOT"/etc/buildenv/run-docker.sh $DIR/build.sh "$@"
# fi
