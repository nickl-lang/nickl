#!/bin/sh

set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

if [ -z ${PLATFORM+x} ]; then
    PLATFORM=linux
fi

. $DIR/common/common-info.sh
. $DIR/$PLATFORM/image-info.sh

PROJECTDIR=$(realpath $DIR/../..)
DOCKERHOME=$PROJECTDIR/out/home

mkdir -p $DOCKERHOME

if [ -t 0 ]; then
    TTY_ARG="-ti"
else
    TTY_ARG=""
fi

if [ -z "$(docker images -q $IMAGE 2> /dev/null)" ]; then
    if docker pull $DOCKER_REGISTRY_URL/$IMAGE 2> /dev/null; then
        docker image tag $DOCKER_REGISTRY_URL/$IMAGE $IMAGE
        docker image rm $DOCKER_REGISTRY_URL/$IMAGE
    else
        $DIR/build-image.sh
    fi
fi

echo "Running docker image $IMAGE"

docker run \
    $TTY_ARG \
    --rm \
    -h $(hostname) \
    -v /etc/passwd:/etc/passwd:ro \
    -v /etc/group:/etc/group:ro \
    -v /etc/shadow:/etc/shadow:ro \
    -v $HOME/.Xauthority:$HOME/.Xauthority \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -e DISPLAY \
    -e TERM \
    -e PLATFORM \
    -e DEV_BUILD \
    -e BUILD_TYPE \
    -e EXTRA_CMAKE_ARGS \
    -u $(id -u):$(id -g) \
    -w $PROJECTDIR \
    -v $DOCKERHOME:$HOME \
    -v $PROJECTDIR:$PROJECTDIR \
    $EXTRA_DOCKER_OPTS \
    $IMAGE "$@"
