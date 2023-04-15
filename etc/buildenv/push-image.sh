#!/bin/sh

set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

if [ -z ${PLATFORM+x} ]; then
    PLATFORM=linux
fi

COMMON_ROOT=$DIR/common
PLATFORM_ROOT=$DIR/$PLATFORM

. $COMMON_ROOT/common-info.sh
. $PLATFORM_ROOT/image-info.sh

docker image tag $IMAGE $DOCKER_REGISTRY_URL/$IMAGE
docker image tag $IMAGE $DOCKER_REGISTRY_URL/$IMAGE_NAME:latest

docker push $DOCKER_REGISTRY_URL/$IMAGE
docker push $DOCKER_REGISTRY_URL/$IMAGE_NAME:latest

docker image rm $DOCKER_REGISTRY_URL/$IMAGE
docker image rm $DOCKER_REGISTRY_URL/$IMAGE_NAME:latest
