#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

. "$DIR/config.sh"

PROJECT_DIR=$(realpath "$DIR/../..")
DOCKER_HOME="$PROJECT_DIR/out/home"

if [ -z "$(docker images -q "$TAG" 2> /dev/null)" ]; then
  if docker pull "$REMOTE/$TAG" 2> /dev/null; then
    docker image tag "$REMOTE/$TAG" "$TAG"
    docker image rm "$REMOTE/$TAG"
  else
    "$DIR/build-image.sh" -i "$IMAGE"
  fi
fi

echo >&2 "INFO: Running docker image '$TAG'"

mkdir -p "$DOCKER_HOME"

if [ -t 0 ]; then
  TTY_ARG="-ti"
else
  TTY_ARG=""
fi

TAG_PRINTABLE=$(echo "$TAG" | sed 's/:/-/g')
PASSWD_FILE=/etc/passwd
GROUPS_FILE=/etc/group
DOCKER_PASSWD_FILE="/tmp/passwd-$TAG_PRINTABLE"
DOCKER_GROUPS_FILE="/tmp/group-$TAG_PRINTABLE"

if [ ! -e "$DOCKER_PASSWD_FILE" ] || [ ! -e "$DOCKER_GROUPS_FILE" ]; then
  ID=$(docker create "$TAG")
  if [ ! -e "$DOCKER_PASSWD_FILE" ]; then
    docker cp "$ID:$PASSWD_FILE" "$DOCKER_PASSWD_FILE"
    getent passwd "$(id -u)" >> "$DOCKER_PASSWD_FILE"
  fi
  if [ ! -e "$DOCKER_GROUPS_FILE" ]; then
    docker cp "$ID:$GROUPS_FILE" "$DOCKER_GROUPS_FILE"
    echo "$(id -g -n):x:$(id -g):" >> "$DOCKER_GROUPS_FILE"
  fi
  docker rm "$ID" > /dev/null
fi

XSESSION_FILE="$HOME/.Xauthority"
XSESSION_FILE_ARG="-v $XSESSION_FILE:$XSESSION_FILE"
DOCKER_XSESSION_FILE="$DOCKER_HOME/$(basename "$XSESSION_FILE")"
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
  -h "$(hostname)" \
  -v "$DOCKER_PASSWD_FILE:$PASSWD_FILE:ro" \
  -v "$DOCKER_GROUPS_FILE:$GROUPS_FILE:ro" \
  $XSESSION_FILE_ARG \
  $X11_SOCKET_ARG \
  -e BUILD_TYPE \
  -e EXTRA_CMAKE_ARGS \
  -e MAKE \
  -e TERM \
  -u "$(id -u):$(id -g)" \
  -w "$PROJECT_DIR" \
  -v "$DOCKER_HOME:$HOME" \
  -v "$PROJECT_DIR:$PROJECT_DIR" \
  $EXTRA_DOCKER_OPTS \
  "$TAG" "$@"
