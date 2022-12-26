#!/bin/sh

set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

if [ -z ${PLATFORM+x} ]; then
    PLATFORM=linux
fi

. $DIR/$PLATFORM/image-info.sh

PROJECTDIR=$(realpath $DIR/../..)
DOCKERHOME=$PROJECTDIR/out/home

mkdir -p $DOCKERHOME

if [ -z "$(docker images -q $IMAGE 2> /dev/null)" ]; then
    URL=ghcr.io/nk4rter
    if docker pull $URL/$IMAGE; then
        docker image tag $URL/$IMAGE $IMAGE
        docker image rm $URL/$IMAGE
    else
        $DIR/build-image.sh
    fi
fi

echo "Running docker image $IMAGE"

docker run \
    -ti \
    --rm \
    -h $(hostname) \
    --device=/dev/dri/card0:/dev/dri/card0 \
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
    -u $(id -u):$(id -g) \
    -w $PROJECTDIR \
    -v $DOCKERHOME:$HOME \
    -v $PROJECTDIR:$PROJECTDIR \
    $IMAGE "$@"
