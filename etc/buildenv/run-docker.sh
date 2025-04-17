#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

. "$DIR/config.sh"

if [ -z "$(docker images -q "$IMAGE_TAG" 2>/dev/null)" ]; then
  echo >&2 "INFO: Trying to pull docker image '$IMAGE_TAG'"
  if docker pull "$REMOTE/$IMAGE_TAG" 2>/dev/null; then
    docker image tag "$REMOTE/$IMAGE_TAG" "$IMAGE_TAG"
    docker image rm "$REMOTE/$IMAGE_TAG"
  else
    "$DIR/build-image.sh" -i "$IMAGE"
  fi
fi

CONTAINER_NAME="$IMAGE_NAME-$IMAGE_VERSION"

[ -z "$(docker ps -a -q -f name="$CONTAINER_NAME")" ] && {
  echo >&2 "INFO: Creating docker container '$CONTAINER_NAME'"

  REPO_ROOT=$(realpath "$DIR/../..")
  DOCKER_HOME="$REPO_ROOT/out/home"

  mkdir -p "$DOCKER_HOME"

  [ -n "${XAUTHORITY+x}" ] &&
    XAUTHORITY_ARG="--env XAUTHORITY --volume $XAUTHORITY:$XAUTHORITY"

  X11_SOCKET="/tmp/.X11-unix"
  [ -e $X11_SOCKET ] &&
    X11_SOCKET_ARG="--volume $X11_SOCKET:$X11_SOCKET"

  [ -n "${XDG_RUNTIME_DIR+x}" ] &&
    XDG_RUNTIME_DIR_ARG="--env XDG_RUNTIME_DIR --volume "$XDG_RUNTIME_DIR:$XDG_RUNTIME_DIR""

  USER_ID=$(id -u)
  GROUP_ID=$(id -g)
  USER=$(id -u -n)
  GROUP=$(id -g -n)

  docker run \
    --detach \
    --interactive \
    --hostname "$(hostname)" \
    --user "$USER_ID:$GROUP_ID" \
    --workdir "$REPO_ROOT" \
    --env HOME \
    --env USER \
    $XAUTHORITY_ARG \
    $X11_SOCKET_ARG \
    $XDG_RUNTIME_DIR_ARG \
    --volume "$DOCKER_HOME:$HOME" \
    --volume "$REPO_ROOT:$REPO_ROOT" \
    --name "$CONTAINER_NAME" \
    $EXTRA_DOCKER_OPTS \
    "$IMAGE_TAG" >/dev/null

  docker exec --user 0:0 "$CONTAINER_NAME" sh -c "
    echo '$USER:x:$USER_ID:$GROUP_ID::$HOME:/usr/bin/bash' >>/etc/passwd &&
    echo '$GROUP:x:$GROUP_ID:' >>/etc/group &&
    echo '$USER:*:0:0:99999:7:::' >>/etc/shadow &&
    echo '%$GROUP ALL=(ALL) NOPASSWD:ALL' >/etc/sudoers.d/user &&
    chmod 0440 /etc/sudoers.d/user
  "
}

[ -z "$(docker ps -q -f name="$CONTAINER_NAME")" ] && {
  echo >&2 "INFO: Starting docker container '$CONTAINER_NAME'"
  docker start "$CONTAINER_NAME" >/dev/null
}

echo >&2 "INFO: Running in docker container '$CONTAINER_NAME'"

[ -t 0 ] && TTY_ARG="--tty"

CMD="$*"
[ -z "$CMD" ] && CMD=bash

docker exec \
  $TTY_ARG \
  --interactive \
  --env BUILD_TYPE \
  --env EXTRA_CMAKE_ARGS \
  --env MAKE \
  --env DISPLAY \
  --env TERM \
  "$CONTAINER_NAME" \
  $CMD
