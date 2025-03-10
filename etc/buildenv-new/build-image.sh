#!/bin/sh

set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

if [ -z "$TARGET" ]; then
  TARGET=linux
fi

IMAGE=buildenv-nickl-new-$TARGET:latest

cat \
  $DIR/Dockerfile.base \
  $DIR/Dockerfile.cmake \
  $DIR/Dockerfile.common \
  $DIR/Dockerfile.toolchain-common \
  $DIR/Dockerfile.toolchain-linux \
  $DIR/Dockerfile.toolchain-mingw-common \
  $DIR/Dockerfile.toolchain-mingw \
  $DIR/Dockerfile.toolchain-mingw-native \
  $DIR/Dockerfile.mingw \
  $DIR/Dockerfile.linux \
|
  docker build -t $IMAGE -f - --target $TARGET $@ $DIR

