#!/bin/sh

set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

if [ -z "$TARGET" ]; then
  TARGET=linux
fi

IMAGE=buildenv-nickl-new-$TARGET:latest

cat \
  $DIR/Dockerfile.common \
  $DIR/Dockerfile.toolchain-linux \
  $DIR/Dockerfile.linux \
  $DIR/Dockerfile.gdb-linux \
  $DIR/Dockerfile.linux-dev \
  $DIR/Dockerfile.toolchain-mingw \
  $DIR/Dockerfile.mingw \
  $DIR/Dockerfile.toolchain-mingw-native \
  $DIR/Dockerfile.gdb-mingw \
  $DIR/Dockerfile.mingw-dev \
| docker build \
    -t $IMAGE -f - --target $TARGET $EXTRA_DOCKER_OPTS $@ $DIR
