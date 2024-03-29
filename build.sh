#!/bin/sh

set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

PROJ_ROOT=$DIR

if [ -f /.dockerenv ]; then
    if [ -z ${PLATFORM+x} ]; then
        PLATFORM=linux
    fi

    if [ -z "$BUILD_TYPE" ]; then
        BUILD_TYPE=Debug
    fi

    PLATFORM_ROOT=$PROJ_ROOT/etc/buildenv/$PLATFORM
    if [ -f $PLATFORM_ROOT/scripts/prepare_buildenv.sh ]; then
        $PLATFORM_ROOT/scripts/prepare_buildenv.sh
    fi

    PLATFORM_SUFFIX=$PLATFORM-$(echo $BUILD_TYPE | tr '[:upper:]' '[:lower:]')
    BIN_DIR=$PROJ_ROOT/out/build-$PLATFORM_SUFFIX
    mkdir -p $BIN_DIR

    if [ ! -f $BIN_DIR/build.ninja -o \
         ! -f $BIN_DIR/CMakeCache.txt -o \
         -n "$DEV_BUILD" ]; then
        cmake -S $PROJ_ROOT -B $BIN_DIR -GNinja \
            -DCMAKE_TOOLCHAIN_FILE=$PLATFORM_ROOT/cmake/Toolchain.cmake \
            -DCMAKE_INSTALL_PREFIX=$PROJ_ROOT/out/install-$PLATFORM_SUFFIX \
            -DDEPLOY_PREFIX=$PROJ_ROOT/out/deploy-$PLATFORM_SUFFIX \
            -DPLATFORM=$PLATFORM \
            -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
            ${DEV_BUILD+-DDEV_BUILD=$DEV_BUILD} \
            $EXTRA_CMAKE_ARGS
    fi

    export CTEST_PARALLEL_LEVEL=$(nproc)

    ninja -C $BIN_DIR "$@"
else
    $PROJ_ROOT/etc/buildenv/run-docker.sh $DIR/build.sh "$@"
fi
