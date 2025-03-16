#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

. "$DIR/config.sh"

echo >&2 "INFO: Building docker image '$IMAGE'"

maybe_tee() {
  if [ "$DEBUG" = true ]; then
    tee Dockerfile.debug
  else
    cat -
  fi
}

echo "$STAGES" | xargs -I{} cat "$DIR/src/Dockerfile.{}" | maybe_tee |
  docker build \
    -t "$IMAGE" \
    -f - \
    --target "$TARGET" \
    "$@" \
    "$DIR/src"
