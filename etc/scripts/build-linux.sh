#!/bin/sh
PLATFORM=linux
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
PROJ_ROOT="$DIR/../.."
BIN_DIR="$PROJ_ROOT/out/build-$PLATFORM"
cmake -S $PROJ_ROOT -B $BIN_DIR -GNinja
ninja -C $BIN_DIR
