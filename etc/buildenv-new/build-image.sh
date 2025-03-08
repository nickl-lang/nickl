#!/bin/sh

set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

if [ -z "$IMAGE" ]; then
  IMAGE=linux
fi

IMAGE_NAME=buildenv-nickl-new-$IMAGE:latest

docker build -t $IMAGE_NAME -f $DIR/Dockerfile.$IMAGE $@ $DIR
