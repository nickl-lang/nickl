#!/bin/sh
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
PROJECTDIR=$(realpath $DIR/../../..)
DOCKERHOME=$PROJECTDIR/out/home
mkdir -p $DOCKERHOME
. $DIR/image-info.sh
if [ -z "$(docker images -q $IMAGE_TAG 2> /dev/null)" ]; then
    IMAGE_URL=ghcr.io/nk4rter/$IMAGE_TAG
    docker pull $IMAGE_URL
    docker tag $IMAGE_URL $IMAGE_TAG
    docker image rm $IMAGE_URL
fi
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
    -u $(id -u):$(id -g) \
    -w $PROJECTDIR \
    -v $DOCKERHOME:$HOME \
    -v $PROJECTDIR:$PROJECTDIR \
    $IMAGE_TAG "$@"
