#!/bin/sh

set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

PROJ_ROOT=$DIR

if [ -f /.dockerenv ]; then
    if [ -z ${PLATFORM+x} ]; then
        echo "$0: error, PLATFORM is not set"
        exit 1
    fi

    if [ -z "$BUILD_TYPE" ]; then
        BUILD_TYPE=Release
    fi

    PLATFORM_SUFFIX=$PLATFORM-$(echo $BUILD_TYPE | tr '[:upper:]' '[:lower:]')
    BIN_DIR=$PROJ_ROOT/out/build-$PLATFORM_SUFFIX
    mkdir -p $BIN_DIR

    if [ ! -f $BIN_DIR/build.ninja -o \
         ! -f $BIN_DIR/CMakeCache.txt ]; then
        cmake -S $PROJ_ROOT -B $BIN_DIR -GNinja \
            -DCMAKE_TOOLCHAIN_FILE=$PROJ_ROOT/etc/buildenv/$PLATFORM/cmake/Toolchain.cmake \
            -DCMAKE_INSTALL_PREFIX=$PROJ_ROOT/out/install-$PLATFORM_SUFFIX \
            -DPLATFORM=$PLATFORM \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE
    fi

    ninja -C $BIN_DIR "$@"
else
    $PROJ_ROOT/etc/buildenv/run-docker.sh $DIR/build.sh "$@"
fi
