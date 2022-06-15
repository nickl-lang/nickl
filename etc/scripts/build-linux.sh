#!/bin/sh
PLATFORM=linux
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
PROJ_ROOT=$(realpath $DIR/../..)
BIN_DIR=$(realpath $PROJ_ROOT/out/build-$PLATFORM)
$PROJ_ROOT/etc/buildenv/$PLATFORM/run-docker.sh cmake -S $PROJ_ROOT -B $BIN_DIR -GNinja
$PROJ_ROOT/etc/buildenv/$PLATFORM/run-docker.sh ninja -C $BIN_DIR
