#!/bin/sh

set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

if [ -z "$TARGET" ]; then
  TARGET=linux
fi

IMAGE=buildenv-nickl-new-$TARGET:latest

docker build -t $IMAGE --target $TARGET $@ $DIR
