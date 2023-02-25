#!/bin/sh

set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

if [ -z ${PLATFORM+x} ]; then
    PLATFORM=linux
fi

PLATFORM_ROOT=$DIR/$PLATFORM

. $PLATFORM_ROOT/image-info.sh

cat $PLATFORM_ROOT/src/Dockerfile |
    DOCKER_BUILDKIT=1 docker build -t $IMAGE -f - $DIR
