#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

. "$DIR/config.sh"

if [ -z "$(docker images -q "$TAG" 2>/dev/null)" ]; then
  echo >&2 "INFO: Trying to pull docker image '$TAG'"
  if docker pull "$REMOTE/$TAG" 2>/dev/null; then
    docker image tag "$REMOTE/$TAG" "$TAG"
    docker image rm "$REMOTE/$TAG"
  else
    "$DIR/build-image.sh" -i "$IMAGE"
  fi
fi

CONTAINER_NAME="$IMAGE_NAME-$IMAGE_VERSION"

[ -z "$(docker ps -a -q -f name="$CONTAINER_NAME")" ] && {
  echo >&2 "INFO: Creating docker container '$CONTAINER_NAME'"

  PROJECT_DIR=$(realpath "$DIR/../..")
  DOCKER_HOME="$PROJECT_DIR/out/home"

  mkdir -p "$DOCKER_HOME"

  [ -f "$XAUTHORITY" ] && XAUTHORITY_ARG="--volume $XAUTHORITY:$XAUTHORITY"

  X11_SOCKET="/tmp/.X11-unix"
  [ -e $X11_SOCKET ] && X11_SOCKET_ARG="--volume $X11_SOCKET:$X11_SOCKET"

  docker run \
    --detach \
    --interactive \
    --hostname "$(hostname)" \
    --user "$(id -u):$(id -g)" \
    --workdir "$PROJECT_DIR" \
    --env BUILD_TYPE \
    --env EXTRA_CMAKE_ARGS \
    --env MAKE \
    --env DISPLAY \
    --env HOME \
    --env TERM \
    --env USER \
    --env XAUTHORITY \
    --env XDG_RUNTIME_DIR \
    $XAUTHORITY_ARG \
    $X11_SOCKET_ARG \
    --volume "$DOCKER_HOME:$HOME" \
    --volume "$PROJECT_DIR:$PROJECT_DIR" \
    --volume "$XDG_RUNTIME_DIR:$XDG_RUNTIME_DIR" \
    --name "$CONTAINER_NAME" \
    $EXTRA_DOCKER_OPTS \
    "$TAG" >/dev/null

    docker exec --user 0:0 "$CONTAINER_NAME" sh -c "
      echo '$(id -u -n):x:$(id -u):$(id -g)::$HOME:/usr/bin/bash' >> /etc/passwd &&
      echo '$(id -g -n):x:$(id -g):' >> /etc/group &&
      echo '$(id -u -n):*:0:0:99999:7:::' >> /etc/shadow &&
      echo '%$(id -g -n) ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/user &&
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
  --interactive \
  $TTY_ARG \
  "$CONTAINER_NAME" \
  $CMD
