#!/bin/sh

set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

if [ -z "$TARGET" ]; then
  TARGET=linux
fi

PROJECTDIR=$(realpath $DIR/../..)
DOCKERHOME=$PROJECTDIR/out/home

mkdir -p $DOCKERHOME

IMAGE=buildenv-nickl-new-$TARGET:latest

echo "Running docker image $IMAGE"

if [ -t 0 ]; then
  TTY_ARG="-ti"
else
  TTY_ARG=""
fi

IMAGE_PRINTABLE=$(echo $IMAGE | sed 's/:/-/g')
PASSWD_FILE=/etc/passwd
GROUPS_FILE=/etc/group
DOCKER_PASSWD_FILE=/tmp/passwd-$IMAGE_PRINTABLE
DOCKER_GROUPS_FILE=/tmp/group-$IMAGE_PRINTABLE

if [ ! -e $DOCKER_PASSWD_FILE -o ! -e $DOCKER_GROUPS_FILE ]; then
    ID=$(docker create $IMAGE)
    if [ ! -e $DOCKER_PASSWD_FILE ]; then
        docker cp $ID:$PASSWD_FILE $DOCKER_PASSWD_FILE
        echo "$(getent passwd $(id -u))" >> $DOCKER_PASSWD_FILE
    fi
    if [ ! -e $DOCKER_GROUPS_FILE ]; then
        docker cp $ID:$GROUPS_FILE $DOCKER_GROUPS_FILE
        echo "$(id -g -n):x:$(id -g):" >> $DOCKER_GROUPS_FILE
    fi
    docker rm $ID > /dev/null
fi

XSESSION_FILE="$HOME/.Xauthority"
XSESSION_FILE_ARG="-v $XSESSION_FILE:$XSESSION_FILE"
DOCKER_XSESSION_FILE="$DOCKERHOME/$(basename "$XSESSION_FILE")"
rm -rf "$DOCKER_XSESSION_FILE"
if [ -f "$XSESSION_FILE" ]; then
    touch "$DOCKER_XSESSION_FILE"
elif [ -d "$XSESSION_FILE" ]; then
    mkdir -p "$DOCKER_XSESSION_FILE"
else
    XSESSION_FILE_ARG=""
fi

X11_SOCKET_FILE="/tmp/.X11-unix"
if [ -e $X11_SOCKET_FILE ]; then
    X11_SOCKET_ARG="-v $X11_SOCKET_FILE:$X11_SOCKET_FILE"
fi

docker run \
  $TTY_ARG \
  --rm \
  -h $(hostname) \
  -v $DOCKER_PASSWD_FILE:$PASSWD_FILE:ro \
  -v $DOCKER_GROUPS_FILE:$GROUPS_FILE:ro \
  $XSESSION_FILE_ARG \
  $X11_SOCKET_ARG \
  -e DISPLAY \
  -e TERM \
  -e EXTRA_CMAKE_ARGS \
  -u $(id -u):$(id -g) \
  -w $PROJECTDIR \
  -v $DOCKERHOME:$HOME \
  -v $PROJECTDIR:$PROJECTDIR \
  $EXTRA_DOCKER_OPTS \
  $IMAGE "$@"
