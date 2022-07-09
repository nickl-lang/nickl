#!/bin/sh
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
. $DIR/image-info.sh
cat $DIR/src/Dockerfile.base |
    DOCKER_BUILDKIT=1 docker build -t $IMAGE_TAG $DIR -f -
