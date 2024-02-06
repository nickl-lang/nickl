#!/bin/sh

set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

if [ -z ${PLATFORM+x} ]; then
    PLATFORM=linux
fi

COMMON_ROOT=$DIR/common
PLATFORM_ROOT=$DIR/$PLATFORM

. $PLATFORM_ROOT/image-info.sh

cat $COMMON_ROOT/src/Dockerfile $PLATFORM_ROOT/src/Dockerfile |
    docker build -t $IMAGE -f - $EXTRA_DOCKER_OPTS $DIR
